// Windows backend: the GATT server and LE advertising over WinRT.

#include "GMBluetooth_windows_internal.h"

#if defined(_WIN32)

namespace gmbluetooth::win
{
    namespace
    {
        // The central a GATT server event carries: the key the core maps to a
        // server connection, and the device address when known, with the id a
        // scan result for the same peer has (R1-155): typed when a scan saw
        // the address.
        LeServerCentral server_central(SharedLeState& le, const std::string& key, const std::string& address)
        {
            LeServerCentral central;
            central.key = key;
            if (!address.empty())
            {
                BTH_ADDR parsed = 0;
                std::optional<WDB::BluetoothAddressType> type;
                if (parse_bluetooth_address(address, parsed))
                    type = find_address_type(le, static_cast<std::uint64_t>(parsed));
                central.address = address;
                central.device_id = le_device_id(address, type);
            }
            return central;
        }

        // BluetoothLeAttributePermission onto Windows protection levels. READ
        // and WRITE are implied by the characteristic properties; the signed
        // write bits never get here (le_server_add_service refuses them).
        WDBG::GattProtectionLevel read_protection_from_permissions(std::int32_t permissions)
        {
            if ((permissions & kPermissionReadEncryptedMitm) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & kPermissionReadEncrypted) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
            return WDBG::GattProtectionLevel::Plain;
        }

        WDBG::GattProtectionLevel write_protection_from_permissions(std::int32_t permissions)
        {
            if ((permissions & kPermissionWriteEncryptedMitm) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & kPermissionWriteEncrypted) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
            return WDBG::GattProtectionLevel::Plain;
        }

        constexpr std::size_t k_max_queued_notifies = 64;

        using NotifyOperation = WF::IAsyncOperation<
            WF::Collections::IVectorView<WDBG::GattClientNotificationResult>>;

        // The call is a sync int with no callback, so a client that failed
        // can only be logged.
        void log_notify_failures(const NotifyOperation& operation, WF::AsyncStatus status)
        {
            try
            {
                if (status != WF::AsyncStatus::Completed)
                {
                    GMBT_LOG("GATT notify did not complete: status=%d", static_cast<int>(status));
                    return;
                }

                for (const auto& result : operation.GetResults())
                {
                    if (result.Status() == WDBG::GattCommunicationStatus::Success)
                        continue;

                    GMBT_LOG(
                        "GATT notify failed for a subscriber: status=%d",
                        static_cast<int>(result.Status()));
                }
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG("GATT notify failed: %s", winrt::to_string(error.message()).c_str());
            }
        }

        void start_next_notify(const std::shared_ptr<NotifyQueueState>& notifies)
        {
            for (;;)
            {
                PendingNotify next;
                {
                    std::scoped_lock lock(notifies->mutex);
                    if (notifies->queue.empty())
                    {
                        notifies->in_flight = false;
                        return;
                    }
                    next = std::move(notifies->queue.front());
                    notifies->queue.pop_front();
                    notifies->in_flight = true;
                }

                try
                {
                    if (next.client)
                    {
                        auto operation = next.characteristic.NotifyValueAsync(bytes_to_buffer(next.bytes), next.client);
                        operation.Completed(
                            [notifies](const WF::IAsyncOperation<WDBG::GattClientNotificationResult>& done, WF::AsyncStatus status)
                            {
                                try
                                {
                                    if (status != WF::AsyncStatus::Completed)
                                        GMBT_LOG("GATT notify did not complete: status=%d", static_cast<int>(status));
                                    else if (done.GetResults().Status() != WDBG::GattCommunicationStatus::Success)
                                        GMBT_LOG("GATT notify failed for its central: status=%d", static_cast<int>(done.GetResults().Status()));
                                }
                                catch (const winrt::hresult_error& error)
                                {
                                    GMBT_LOG("GATT notify failed: %s", winrt::to_string(error.message()).c_str());
                                }
                                start_next_notify(notifies);
                            });
                        return;
                    }

                    auto operation = next.characteristic.NotifyValueAsync(bytes_to_buffer(next.bytes));
                    operation.Completed(
                        [notifies](const NotifyOperation& done, WF::AsyncStatus status)
                        {
                            log_notify_failures(done, status);
                            start_next_notify(notifies);
                        });
                    return;
                }
                catch (const winrt::hresult_error& error)
                {
                    // The characteristic went away (server stopped, services
                    // cleared): drop this one and go on with the queue.
                    GMBT_LOG("GATT notify could not start: %s", winrt::to_string(error.message()).c_str());
                }
            }
        }

        void clear_queued_notifies(const std::shared_ptr<NotifyQueueState>& notifies)
        {
            std::scoped_lock lock(notifies->mutex);
            notifies->queue.clear();
        }

        // A local GATT service as le_server_add_service parsed it on the game
        // thread, for the worker to create.
        struct LocalDescriptorDefinition
        {
            std::uint64_t id = 0;
            winrt::guid guid{};
            std::string uuid;
            // Served by Windows, never reaching on_read (R2-90).
            std::optional<std::vector<std::uint8_t>> value;
        };

        struct LocalCharacteristicDefinition
        {
            std::uint64_t id = 0;
            winrt::guid guid{};
            std::string uuid;
            std::int32_t properties = 0;
            std::int32_t permissions = 0;
            std::vector<std::uint8_t> value;
            // A User Description (0x2901), which Windows publishes from the
            // characteristic's parameters and not as a descriptor (R2-12).
            std::optional<std::string> user_description;
            std::vector<LocalDescriptorDefinition> descriptors;
        };

        constexpr std::string_view k_user_description_uuid = "00002901-0000-1000-8000-00805f9b34fb";

        struct LocalServiceDefinition
        {
            std::uint64_t id = 0;
            winrt::guid guid{};
            std::string uuid;
            std::vector<LocalCharacteristicDefinition> characteristics;
        };

        void push_server_connection_state(
            const std::shared_ptr<SharedLeState>& le,
            const std::string& central,
            const std::string& address,
            bool connected)
        {
            if (central.empty())
                return;

            BackendEvent event;
            event.type = BackendEventType::LeServerCentral;
            event.transport = Transport::LowEnergy;
            event.central = server_central(*le, central, address);
            event.connected = connected;
            push_le_event(le, std::move(event));
        }

        void server_session_closed(
            const std::shared_ptr<SharedLeState>& le,
            const std::string& central,
            const std::string& address)
        {
            ServerSession entry;
            {
                std::scoped_lock lock(le->server_sessions_mutex);
                const auto it = le->server_sessions.find(central);
                if (it == le->server_sessions.end())
                    return;
                entry = std::move(it->second);
                le->server_sessions.erase(it);
            }

            try
            {
                entry.session.SessionStatusChanged(entry.status_token);
            }
            catch (...)
            {
            }
            push_server_connection_state(le, central, address, false);
        }

        // The key the core knows the central by - its session's device id -
        // registering the session on first sight; is_new says it was. Empty for
        // a session that names no device.
        std::string note_server_session(
            const std::shared_ptr<SharedLeState>& le,
            const WDBG::GattSession& session,
            std::string& address,
            bool& is_new)
        {
            is_new = false;
            if (!session)
                return std::string();

            std::string central;
            try
            {
                central = winrt::to_string(session.DeviceId().Id());
            }
            catch (...)
            {
                return std::string();
            }
            address = address_from_device_id(central);

            std::scoped_lock lock(le->server_sessions_mutex);
            if (le->server_sessions.find(central) != le->server_sessions.end())
                return central;

            ServerSession entry;
            entry.session = session;
            try
            {
                std::weak_ptr<SharedLeState> weak = le;
                entry.status_token = session.SessionStatusChanged(
                    [weak, central, address](const WDBG::GattSession&, const WDBG::GattSessionStatusChangedEventArgs& args)
                    {
                        const auto current = weak.lock();
                        if (current && current->alive.load() && args.Status() == WDBG::GattSessionStatus::Closed)
                            server_session_closed(current, central, address);
                    });
            }
            catch (...)
            {
            }
            le->server_sessions.emplace(central, std::move(entry));
            is_new = true;
            return central;
        }

        // The server stopped: the core retires every central itself, so the
        // sessions are only let go.
        void release_server_sessions_noexcept(const std::shared_ptr<SharedLeState>& le) noexcept
        {
            std::unordered_map<std::string, ServerSession> sessions;
            {
                std::scoped_lock lock(le->server_sessions_mutex);
                sessions.swap(le->server_sessions);
            }

            for (auto& [_, entry] : sessions)
            {
                try
                {
                    entry.session.SessionStatusChanged(entry.status_token);
                }
                catch (...)
                {
                }
            }
        }

        // Caller holds le.services_mutex.
        void start_service_advertising_locked(
            SharedLeState& le,
            const std::shared_ptr<LocalGattServiceState>& service)
        {
            if (!service || !service->provider)
                return;
            if (le.advertise_service_uuids.find(service->uuid) == le.advertise_service_uuids.end())
                return;

            // Always discoverable, whatever include_name says: Windows puts the
            // service UUID in the advertisement only for a discoverable service,
            // which also carries the name. A provider that is not discoverable
            // stays Created and never reports a status, so a start waiting on
            // it would wait forever.
            WDBG::GattServiceProviderAdvertisingParameters parameters;
            parameters.IsConnectable(le.advertise_connectable);
            parameters.IsDiscoverable(true);

            const auto data_it = le.advertise_service_data.find(service->uuid);
            if (data_it != le.advertise_service_data.end() && !data_it->second.empty())
                parameters.ServiceData(bytes_to_buffer(data_it->second));

            service->provider.StartAdvertising(parameters);
        }

        void revoke_advertise_handlers(const std::shared_ptr<AdvertiseStartTracker>& tracker) noexcept
        {
            std::vector<std::pair<WDBG::GattServiceProvider, winrt::event_token>> providers;
            {
                std::scoped_lock lock(tracker->mutex);
                providers.swap(tracker->providers);
            }

            for (auto& [provider, token] : providers)
            {
                try
                {
                    provider.AdvertisementStatusChanged(token);
                }
                catch (...)
                {
                }
            }
        }

        // Completes the start once. False when it was already completed. A
        // start that succeeded keeps its handlers for the advertisement's
        // life, so a later stop is heard (R2-10).
        bool finish_advertise_start(
            const std::shared_ptr<SharedLeState>& le,
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            Error error,
            std::string message)
        {
            if (tracker->done.exchange(true))
                return false;

            if (error != Error::Ok)
                revoke_advertise_handlers(tracker);
            complete_le_op(le, tracker->op_id, error, std::move(message));
            return true;
        }

        // Stops the publisher and the providers a start began.
        void stop_advertisers_noexcept(
            const WDBA::BluetoothLEAdvertisementPublisher& publisher,
            const std::vector<WDBG::GattServiceProvider>& providers) noexcept
        {
            try
            {
                if (publisher)
                    publisher.Stop();
            }
            catch (...)
            {
            }

            for (const auto& provider : providers)
            {
                try
                {
                    provider.StopAdvertising();
                }
                catch (...)
                {
                }
            }
        }

        void tracker_advertisers(
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            WDBA::BluetoothLEAdvertisementPublisher& publisher,
            std::vector<WDBG::GattServiceProvider>& providers)
        {
            std::scoped_lock lock(tracker->mutex);
            publisher = tracker->publisher;
            for (const auto& pair : tracker->providers)
                providers.push_back(pair.first);
        }

        // Advertising that had started ended without le_advertise_stop: its
        // publisher or one of its providers stopped or aborted (R2-10).
        // Windows publishes a service only while its provider advertises, so
        // the whole advertisement ends: the newest start reports it once, with
        // le_advertising cleared under advertise_flag_mutex, and stops what
        // else it began. le_advertise_stop bumps the generation first, so its
        // own stops are never reported.
        void advertising_lost(
            const std::shared_ptr<SharedLeState>& le,
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            Error error,
            std::string message)
        {
            {
                std::scoped_lock flag_lock(le->advertise_flag_mutex);
                if (tracker->generation != le->advertise_generation.load() || !le->le_advertising.load())
                    return;
                le->le_advertising.store(false);
            }

            WDBA::BluetoothLEAdvertisementPublisher publisher{nullptr};
            std::vector<WDBG::GattServiceProvider> providers;
            tracker_advertisers(tracker, publisher, providers);
            revoke_advertise_handlers(tracker);
            stop_advertisers_noexcept(publisher, providers);

            BackendEvent event;
            event.type = BackendEventType::LeAdvertiseStopped;
            event.transport = Transport::LowEnergy;
            event.error = error;
            event.message = std::move(message);
            push_le_event(le, std::move(event));
        }

        void advertise_start_progress(
            const std::shared_ptr<SharedLeState>& le,
            const std::shared_ptr<AdvertiseStartTracker>& tracker)
        {
            if (tracker->remaining.fetch_sub(1) == 1)
                finish_advertise_start(le, tracker, Error::Ok, {});
        }

        // A failed start leaves nothing it began advertising, unless a newer
        // start already owns the advertisers.
        void advertise_start_failed(
            const std::shared_ptr<SharedLeState>& le,
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            Error error,
            std::string message)
        {
            WDBA::BluetoothLEAdvertisementPublisher publisher{nullptr};
            std::vector<WDBG::GattServiceProvider> providers;
            tracker_advertisers(tracker, publisher, providers);

            if (!finish_advertise_start(le, tracker, error, std::move(message)))
                return;

            {
                std::scoped_lock flag_lock(le->advertise_flag_mutex);
                if (tracker->generation != le->advertise_generation.load())
                    return;
                le->le_advertising.store(false);
            }
            stop_advertisers_noexcept(publisher, providers);
        }

        // A publisher or provider status that ends advertising: before the
        // start completed it fails the start, after it the advertisement.
        void advertiser_ended(
            const std::shared_ptr<SharedLeState>& le,
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            Error error,
            std::string message)
        {
            if (tracker->done.load())
                advertising_lost(le, tracker, error, std::move(message));
            else
                advertise_start_failed(le, tracker, error, std::move(message));
        }

        // BluetoothLeAdvertiseTxPower in dBm: the values Android documents for
        // its four AdvertiseSettings levels.
        std::int16_t advertise_tx_power_dbm(LeAdvertiseTxPower level)
        {
            switch (level)
            {
                case LeAdvertiseTxPower::UltraLow: return -21;
                case LeAdvertiseTxPower::Low:      return -15;
                case LeAdvertiseTxPower::Medium:   return -7;
                case LeAdvertiseTxPower::High:     return 1;
            }
            return -7;
        }

        // The publisher for what le_advertise_start puts in the advertisement
        // itself: the manufacturer data and the power level.
        WDBA::BluetoothLEAdvertisementPublisher make_advertisement_publisher(
            const LeAdvertiseSettings& settings,
            const LeAdvertiseData& data)
        {
            WDBA::BluetoothLEAdvertisementPublisher publisher;
            for (const auto& manufacturer : data.manufacturer_data)
            {
                publisher.Advertisement().ManufacturerData().Append(
                    WDBA::BluetoothLEManufacturerData(
                        manufacturer.company_id,
                        bytes_to_buffer(manufacturer.data)));
            }

            publisher.IncludeTransmitPowerLevel(data.include_tx_power);
            if (settings.tx_power)
            {
                try
                {
                    publisher.PreferredTransmitPowerLevelInDBm(advertise_tx_power_dbm(*settings.tx_power));
                }
                catch (...)
                {
                    // Older Windows builds may not expose the optional power-level setter.
                }
            }
            return publisher;
        }

        // Reports the publisher's status to the start in flight.
        winrt::event_token watch_publisher_status(
            const WDBA::BluetoothLEAdvertisementPublisher& publisher,
            const std::weak_ptr<SharedLeState>& weak,
            const std::shared_ptr<AdvertiseStartTracker>& tracker)
        {
            return publisher.StatusChanged(
                [weak, tracker](
                    const WDBA::BluetoothLEAdvertisementPublisher&,
                    const WDBA::BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args)
                {
                    const auto le = weak.lock();
                    if (!le || !le->alive.load())
                        return;

                    // Only the newest start's publisher may say advertising
                    // ended; an older one's late event leaves it (R1-242).
                    const auto status = args.Status();
                    if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Started)
                    {
                        if (!tracker->done.load())
                            advertise_start_progress(le, tracker);
                    }
                    else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Aborted)
                    {
                        advertiser_ended(
                            le,
                            tracker,
                            map_bluetooth_failure(args.Error()),
                            "Windows aborted the BLE advertisement: " + bluetooth_failure_message(args.Error()));
                    }
                    else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Stopped)
                    {
                        advertiser_ended(
                            le,
                            tracker,
                            Error::OperationFailed,
                            tracker->done.load()
                                ? "Windows stopped the BLE advertisement"
                                : "BLE advertisement stopped before it started");
                    }
                });
        }

        // Reports a service provider's advertising status to the start in
        // flight.
        winrt::event_token watch_provider_status(
            const WDBG::GattServiceProvider& provider,
            const std::weak_ptr<SharedLeState>& weak,
            const std::shared_ptr<AdvertiseStartTracker>& tracker)
        {
            return provider.AdvertisementStatusChanged(
                [weak, tracker](
                    const WDBG::GattServiceProvider&,
                    const WDBG::GattServiceProviderAdvertisementStatusChangedEventArgs& args)
                {
                    const auto le = weak.lock();
                    if (!le || !le->alive.load())
                        return;

                    switch (args.Status())
                    {
                        case WDBG::GattServiceProviderAdvertisementStatus::Started:
                            if (!tracker->done.load())
                                advertise_start_progress(le, tracker);
                            break;
                        case WDBG::GattServiceProviderAdvertisementStatus::StartedWithoutAllAdvertisementData:
                            GMBT_LOG("GATT service advertising started without all of its advertisement data");
                            if (!tracker->done.load())
                                advertise_start_progress(le, tracker);
                            break;
                        case WDBG::GattServiceProviderAdvertisementStatus::Aborted:
                            advertiser_ended(
                                le,
                                tracker,
                                map_bluetooth_failure(args.Error()),
                                "Windows aborted GATT service advertising: " + bluetooth_failure_message(args.Error()));
                            break;
                        case WDBG::GattServiceProviderAdvertisementStatus::Stopped:
                            advertiser_ended(
                                le,
                                tracker,
                                Error::OperationFailed,
                                tracker->done.load()
                                    ? "GATT service advertising stopped"
                                    : "GATT service advertising stopped before it started");
                            break;
                        default:
                            break;
                    }
                });
        }

        // A read request on a local characteristic, or on one of its
        // descriptors when descriptor_id is non-zero; on a WinRT thread.
        void on_local_read_request(
            const std::shared_ptr<SharedLeState>& le,
            std::uint64_t characteristic_id,
            std::uint64_t descriptor_id,
            const WDBG::GattReadRequestedEventArgs& args)
        {
            const auto deferral = args.GetDeferral();
            try
            {
                if (!le || !le->alive.load())
                {
                    deferral.Complete();
                    return;
                }

                const auto request = args.GetRequestAsync().get();
                if (!request)
                {
                    deferral.Complete();
                    return;
                }

                const std::int32_t request_id = le->next_gatt_request_id.fetch_add(1);
                {
                    std::scoped_lock lock(le->gatt_request_mutex);
                    le->pending_gatt_reads[request_id] = PendingGattRead{request, deferral};
                }

                std::string central_address;
                bool central_is_new = false;
                const std::string central = note_server_session(le, args.Session(), central_address, central_is_new);

                BackendEvent event;
                event.type = BackendEventType::LeServerReadRequest;
                event.transport = Transport::LowEnergy;
                event.request_id = request_id;
                event.central = server_central(*le, central, central_address);
                event.attribute.characteristic = characteristic_id;
                event.attribute.descriptor = descriptor_id;
                event.offset = request.Offset();
                push_le_event(le, std::move(event));
            }
            catch (...)
            {
                try { deferral.Complete(); } catch (...) {}
            }
        }

        // A write request on a local characteristic, or on one of its
        // descriptors when descriptor_id is non-zero; on a WinRT thread.
        void on_local_write_request(
            const std::shared_ptr<SharedLeState>& le,
            std::uint64_t characteristic_id,
            std::uint64_t descriptor_id,
            const WDBG::GattWriteRequestedEventArgs& args)
        {
            const auto deferral = args.GetDeferral();
            try
            {
                if (!le || !le->alive.load())
                {
                    deferral.Complete();
                    return;
                }

                const auto request = args.GetRequestAsync().get();
                if (!request)
                {
                    deferral.Complete();
                    return;
                }

                std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
                const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
                const std::int32_t request_id = le->next_gatt_request_id.fetch_add(1);
                // A write without response needs no answer: it is completed now and
                // the core keeps its value for GML, instead of the deferral waiting
                // on a le_server_respond_write nothing obliges the game to send.
                if (with_response)
                {
                    std::scoped_lock lock(le->gatt_request_mutex);
                    le->pending_gatt_writes[request_id] = PendingGattWrite{request, deferral, with_response};
                }
                else
                {
                    deferral.Complete();
                }

                std::string central_address;
                bool central_is_new = false;
                const std::string central = note_server_session(le, args.Session(), central_address, central_is_new);

                BackendEvent event;
                event.type = BackendEventType::LeServerWriteRequest;
                event.transport = Transport::LowEnergy;
                event.request_id = request_id;
                event.central = server_central(*le, central, central_address);
                event.attribute.characteristic = characteristic_id;
                event.attribute.descriptor = descriptor_id;
                event.offset = request.Offset();
                event.bytes = std::move(value);
                event.response_needed = with_response;
                push_le_event(le, std::move(event));
            }
            catch (...)
            {
                try { deferral.Complete(); } catch (...) {}
            }
        }

        // Revokes the read and write request handlers of a local characteristic
        // or descriptor; a token never registered is skipped.
        template <typename Attribute>
        void revoke_request_handlers(
            const Attribute& attribute,
            const winrt::event_token& read_token,
            const winrt::event_token& write_token)
        {
            if (read_token.value != 0)
                attribute.ReadRequested(read_token);
            if (write_token.value != 0)
                attribute.WriteRequested(write_token);
        }

        // Answers the parked request request_id: takes it out of parked
        // (NotFound when it is gone), runs respond on it and completes its
        // deferral. A WinRT failure is logged under kind ("read" or "write")
        // and still completes the deferral.
        template <typename Pending, typename Respond>
        Error respond_parked(
            SharedLeState& le,
            std::unordered_map<std::int32_t, Pending>& parked,
            std::int32_t request_id,
            const char* kind,
            Respond&& respond,
            std::string& message)
        {
            Pending pending;
            {
                std::scoped_lock lock(le.gatt_request_mutex);
                const auto it = parked.find(request_id);
                if (it == parked.end())
                {
                    message = std::string("BLE ") + kind + " request not found";
                    return Error::NotFound;
                }
                pending = it->second;
                parked.erase(it);
            }

            try
            {
                respond(pending);

                if (pending.deferral)
                    pending.deferral.Complete();

                GMBT_TRACE(
                    "GATT %s response completed: request_id=%d final_state=%d",
                    kind,
                    request_id,
                    static_cast<int>(pending.request.State()));

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG(
                    "GATT %s response FAILED: request_id=%d hresult=0x%08X message='%s'",
                    kind,
                    request_id,
                    static_cast<unsigned>(error.code().value),
                    winrt::to_string(error.message()).c_str());
                if (pending.deferral)
                {
                    try { pending.deferral.Complete(); } catch (...) {}
                }
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        // Refuses a parked request nobody will answer, when it is owed an
        // answer, and completes its deferral; failures are ignored.
        template <typename Pending>
        void refuse_parked_noexcept(Pending& pending, bool answer) noexcept
        {
            constexpr std::uint8_t unlikely_error = 0x0E;
            if (answer)
            {
                try { pending.request.RespondWithProtocolError(unlikely_error); } catch (...) {}
            }
            try { if (pending.deferral) pending.deferral.Complete(); } catch (...) {}
        }

        // Stops a service advertising and revokes every handler it registered,
        // so nothing it held fires after it is dropped (R1-105).
        void release_local_service_noexcept(LocalGattServiceState& service) noexcept
        {
            try
            {
                if (service.provider)
                    service.provider.StopAdvertising();
            }
            catch (...)
            {
            }

            for (auto& [_, characteristic] : service.characteristics)
            {
                if (!characteristic || !characteristic->characteristic)
                    continue;

                try
                {
                    revoke_request_handlers(
                        characteristic->characteristic,
                        characteristic->read_token,
                        characteristic->write_token);
                    if (characteristic->subscribed_token.value != 0)
                        characteristic->characteristic.SubscribedClientsChanged(characteristic->subscribed_token);
                }
                catch (...)
                {
                }

                for (auto& descriptor : characteristic->descriptors)
                {
                    if (!descriptor || !descriptor->descriptor)
                        continue;

                    try
                    {
                        revoke_request_handlers(descriptor->descriptor, descriptor->read_token, descriptor->write_token);
                    }
                    catch (...)
                    {
                    }
                }
            }

            service.characteristics.clear();
            service.provider = nullptr;
        }

        // Everything le_server_add_service can refuse, checked on the game
        // thread before any WinRT call.
        Error to_local_service(const LeServerService& service, LocalServiceDefinition& out, std::string& message)
        {
            out.id = service.id;
            if (out.id == 0)
            {
                message = "BLE service definition has no valid id";
                return Error::InvalidArgument;
            }
            std::unordered_set<std::uint64_t> ids{out.id};

            if (service.uuid.empty())
            {
                message = "BLE service UUID is required";
                return Error::InvalidArgument;
            }

            if (!parse_winrt_guid(service.uuid, out.guid))
            {
                message = "Invalid BLE service UUID";
                return Error::InvalidArgument;
            }
            out.uuid = normalize_uuid(service.uuid);

            for (const auto& source : service.characteristics)
            {
                // Windows has no protection level for a signed write.
                if ((source.permissions & (kPermissionWriteSigned | kPermissionWriteSignedMitm)) != 0)
                {
                    message = "Windows has no signed-write permission: "
                        "BluetoothLeAttributePermission.WriteSigned and WriteSignedMitm are not supported";
                    return Error::NotSupported;
                }

                LocalCharacteristicDefinition characteristic;
                if (!parse_winrt_guid(source.uuid, characteristic.guid))
                {
                    message = "Invalid BLE characteristic UUID";
                    return Error::InvalidArgument;
                }
                characteristic.uuid = normalize_uuid(source.uuid);

                characteristic.id = source.id;
                if (characteristic.id == 0 || !ids.insert(characteristic.id).second)
                {
                    message = "BLE characteristic " + characteristic.uuid + " has no valid id";
                    return Error::InvalidArgument;
                }

                characteristic.properties = source.properties;
                characteristic.permissions = source.permissions;
                characteristic.value = source.value;

                for (const auto& source_descriptor : source.descriptors)
                {
                    LocalDescriptorDefinition descriptor;
                    if (!parse_winrt_guid(source_descriptor.uuid, descriptor.guid))
                    {
                        message = "Invalid BLE descriptor UUID";
                        return Error::InvalidArgument;
                    }
                    descriptor.uuid = normalize_uuid(source_descriptor.uuid);

                    // Windows creates the Client Characteristic Configuration
                    // descriptor automatically for Notify/Indicate characteristics.
                    if (descriptor.uuid == "00002902-0000-1000-8000-00805f9b34fb")
                        continue;

                    descriptor.id = source_descriptor.id;
                    if (descriptor.id == 0 || !ids.insert(descriptor.id).second)
                    {
                        message = "BLE descriptor " + descriptor.uuid + " has no valid id";
                        return Error::InvalidArgument;
                    }

                    // CreateDescriptorAsync refuses a User Description, and
                    // the characteristic parameters take only a fixed text.
                    if (descriptor.uuid == k_user_description_uuid)
                    {
                        if (!source_descriptor.value)
                        {
                            message = "Windows publishes a User Description (0x2901) only with a static value";
                            return Error::NotSupported;
                        }
                        characteristic.user_description =
                            std::string(source_descriptor.value->begin(), source_descriptor.value->end());
                        continue;
                    }

                    descriptor.value = source_descriptor.value;
                    characteristic.descriptors.push_back(std::move(descriptor));
                }

                out.characteristics.push_back(std::move(characteristic));
            }

            return Error::Ok;
        }

        // Creates one descriptor of a local characteristic and registers its
        // request handlers. A failure comes back with its message; a WinRT
        // exception goes to the caller.
        Error create_local_descriptor(
            const std::weak_ptr<SharedLeState>& weak,
            LocalGattCharacteristicState& characteristic_state,
            std::uint64_t characteristic_id,
            const LocalDescriptorDefinition& descriptor,
            std::string& message)
        {
            const std::uint64_t descriptor_id = descriptor.id;

            WDBG::GattLocalDescriptorParameters descriptor_parameters;
            descriptor_parameters.ReadProtectionLevel(WDBG::GattProtectionLevel::Plain);
            descriptor_parameters.WriteProtectionLevel(WDBG::GattProtectionLevel::Plain);
            if (descriptor.value)
                descriptor_parameters.StaticValue(bytes_to_buffer(*descriptor.value));

            const auto descriptor_result = characteristic_state.characteristic
                .CreateDescriptorAsync(descriptor.guid, descriptor_parameters).get();
            if (descriptor_result.Error() != WDB::BluetoothError::Success || !descriptor_result.Descriptor())
            {
                message = bluetooth_failure_message(descriptor_result.Error());
                return map_bluetooth_failure(descriptor_result.Error());
            }

            auto descriptor_state = std::make_shared<LocalGattDescriptorState>();
            descriptor_state->descriptor = descriptor_result.Descriptor();
            characteristic_state.descriptors.push_back(descriptor_state);

            descriptor_state->read_token = descriptor_state->descriptor.ReadRequested(
                [weak, characteristic_id, descriptor_id](
                    const WDBG::GattLocalDescriptor&,
                    const WDBG::GattReadRequestedEventArgs& args)
                {
                    on_local_read_request(weak.lock(), characteristic_id, descriptor_id, args);
                });

            descriptor_state->write_token = descriptor_state->descriptor.WriteRequested(
                [weak, characteristic_id, descriptor_id](
                    const WDBG::GattLocalDescriptor&,
                    const WDBG::GattWriteRequestedEventArgs& args)
                {
                    on_local_write_request(weak.lock(), characteristic_id, descriptor_id, args);
                });

            return Error::Ok;
        }

        // Creates one characteristic of a local service with its descriptors,
        // and registers its handlers. It is in the service's map before any
        // handler is registered, so releasing the service after a failure
        // revokes every one already in place. A WinRT exception goes to the
        // caller.
        Error create_local_characteristic(
            const std::weak_ptr<SharedLeState>& weak,
            LocalGattServiceState& service_state,
            const LocalCharacteristicDefinition& characteristic,
            std::string& message)
        {
            const std::uint64_t characteristic_id = characteristic.id;

            WDBG::GattLocalCharacteristicParameters parameters;
            // Windows local GATT rejects Broadcast. All other raw
            // BluetoothLeCharacteristicProperty values map directly.
            parameters.CharacteristicProperties(
                static_cast<WDBG::GattCharacteristicProperties>(characteristic.properties & ~1));
            parameters.ReadProtectionLevel(read_protection_from_permissions(characteristic.permissions));
            parameters.WriteProtectionLevel(write_protection_from_permissions(characteristic.permissions));
            if (!characteristic.value.empty())
                parameters.StaticValue(bytes_to_buffer(characteristic.value));
            if (characteristic.user_description)
                parameters.UserDescription(winrt::to_hstring(*characteristic.user_description));

            const auto characteristic_result = service_state.provider.Service()
                .CreateCharacteristicAsync(characteristic.guid, parameters).get();
            if (characteristic_result.Error() != WDB::BluetoothError::Success || !characteristic_result.Characteristic())
            {
                message = bluetooth_failure_message(characteristic_result.Error());
                return map_bluetooth_failure(characteristic_result.Error());
            }

            auto characteristic_state = std::make_shared<LocalGattCharacteristicState>();
            characteristic_state->characteristic = characteristic_result.Characteristic();
            service_state.characteristics[characteristic_id] = characteristic_state;

            characteristic_state->subscribed_token = characteristic_state->characteristic.SubscribedClientsChanged(
                [weak](const WDBG::GattLocalCharacteristic& local, const WF::IInspectable&)
                {
                    const auto current = weak.lock();
                    if (!current || !current->alive.load())
                        return;

                    try
                    {
                        for (const auto& client : local.SubscribedClients())
                        {
                            std::string address;
                            bool is_new = false;
                            const std::string central = note_server_session(current, client.Session(), address, is_new);
                            if (is_new)
                                push_server_connection_state(current, central, address, true);
                        }
                    }
                    catch (...)
                    {
                    }
                });

            characteristic_state->read_token = characteristic_state->characteristic.ReadRequested(
                [weak, characteristic_id](
                    const WDBG::GattLocalCharacteristic&,
                    const WDBG::GattReadRequestedEventArgs& args)
                {
                    on_local_read_request(weak.lock(), characteristic_id, 0, args);
                });

            characteristic_state->write_token = characteristic_state->characteristic.WriteRequested(
                [weak, characteristic_id](
                    const WDBG::GattLocalCharacteristic&,
                    const WDBG::GattWriteRequestedEventArgs& args)
                {
                    on_local_write_request(weak.lock(), characteristic_id, 0, args);
                });

            for (const auto& descriptor : characteristic.descriptors)
            {
                const Error error = create_local_descriptor(weak, *characteristic_state, characteristic_id, descriptor, message);
                if (error != Error::Ok)
                    return error;
            }

            return Error::Ok;
        }

        // Creates the service le_server_add_service accepted, on the backend
        // worker: each Create*Async waits on WinRT (R1-103). generation is the
        // server generation the add started under.
        void build_local_service(
            const std::shared_ptr<SharedLeState>& le,
            std::uint64_t op_id,
            const LocalServiceDefinition& definition,
            std::uint64_t generation)
        {
            const auto fail = [&le, op_id, &definition](Error error, std::string why)
            {
                {
                    std::scoped_lock lock(le->services_mutex);
                    le->adding_services.erase(definition.uuid);
                }
                complete_le_op(le, op_id, error, std::move(why));
            };

            auto service_state = std::make_shared<LocalGattServiceState>();
            service_state->uuid = definition.uuid;

            try
            {
                const auto provider_result = WDBG::GattServiceProvider::CreateAsync(definition.guid).get();
                if (provider_result.Error() != WDB::BluetoothError::Success || !provider_result.ServiceProvider())
                {
                    fail(map_bluetooth_failure(provider_result.Error()), bluetooth_failure_message(provider_result.Error()));
                    return;
                }
                service_state->provider = provider_result.ServiceProvider();

                const std::weak_ptr<SharedLeState> weak = le;

                for (const auto& characteristic : definition.characteristics)
                {
                    std::string why;
                    const Error error = create_local_characteristic(weak, *service_state, characteristic, why);
                    if (error != Error::Ok)
                    {
                        release_local_service_noexcept(*service_state);
                        fail(error, std::move(why));
                        return;
                    }
                }
            }
            catch (const winrt::hresult_error& error)
            {
                release_local_service_noexcept(*service_state);
                fail(Error::OperationFailed, winrt::to_string(error.message()));
                return;
            }

            bool dropped = false;
            std::string advertise_failure;
            {
                std::scoped_lock lock(le->services_mutex);
                le->adding_services.erase(definition.uuid);
                dropped = le->server_generation != generation || !le->alive.load();
                if (!dropped)
                {
                    le->gatt_services[definition.uuid] = service_state;
                    if (le->le_advertising.load())
                    {
                        try
                        {
                            start_service_advertising_locked(*le, service_state);
                        }
                        catch (const winrt::hresult_error& error)
                        {
                            advertise_failure = winrt::to_string(error.message());
                        }
                    }
                }
            }

            if (dropped)
            {
                release_local_service_noexcept(*service_state);
                complete_le_op(le, op_id, Error::OperationFailed, "The GATT services were cleared before the service was added");
                return;
            }

            if (!advertise_failure.empty())
                GMBT_LOG("The added GATT service could not start advertising: %s", advertise_failure.c_str());
            complete_le_op(le, op_id, Error::Ok, {});
        }
    }

    Error WindowsBackend::le_advertise_start(std::uint64_t op_id, const LeAdvertiseSettings& settings, const LeAdvertiseData& data, std::string& message)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }
        if (!supports_le_advertise())
        {
            message = "BLE peripheral advertising is not supported by this Windows adapter";
            return Error::NotSupported;
        }
        // A second start would ignore its settings and data (R1-188).
        std::uint64_t generation = 0;
        {
            std::scoped_lock flag_lock(le_->advertise_flag_mutex);
            if (le_->le_advertising.exchange(true))
            {
                message = "Bluetooth LE advertising is already running; stop it first";
                return Error::Busy;
            }
            generation = ++le_->advertise_generation;
        }

        std::unique_lock services_lock(le_->services_mutex);

        // The generic Windows advertisement publisher cannot write
        // system-reserved sections such as LocalName or Service UUIDs.
        // Those are published by a registered service's GattServiceProvider,
        // so a service UUID, its service data and the name go out only
        // through one; the publisher carries manufacturer data and the
        // TX power. Each refusal names the field, before anything starts.
        const auto refuse = [this, &services_lock, &message](std::string why)
        {
            services_lock.unlock();
            le_->le_advertising.store(false);
            message = std::move(why);
            return Error::NotSupported;
        };

        std::unordered_set<std::string> service_uuids;
        std::vector<WDBG::GattServiceProvider> providers;
        for (const auto& raw_uuid : data.service_uuids)
        {
            const std::string uuid = normalize_uuid(raw_uuid);
            const auto service_it = le_->gatt_services.find(uuid);
            if (service_it == le_->gatt_services.end() || !service_it->second || !service_it->second->provider)
            {
                return refuse("service_uuids: Windows advertises a service UUID only through a registered GATT service, and " +
                    uuid + " is not one");
            }
            if (service_uuids.insert(uuid).second)
                providers.push_back(service_it->second->provider);
        }

        if (data.include_name && providers.empty())
            return refuse("include_name: Windows advertises the name only through a registered GATT service in service_uuids");

        const bool start_publisher = !data.manufacturer_data.empty() || data.include_tx_power || settings.tx_power.has_value();
        if (!start_publisher && providers.empty())
            return refuse("Windows cannot advertise without manufacturer data, a power level or a registered GATT service");

        try
        {
            le_->advertise_connectable = settings.connectable;
            le_->advertise_service_uuids = std::move(service_uuids);
            le_->advertise_service_data.clear();
            for (const auto& entry : data.service_data)
                le_->advertise_service_data[normalize_uuid(entry.uuid)] = entry.data;

            auto tracker = std::make_shared<AdvertiseStartTracker>();
            tracker->op_id = op_id;
            tracker->generation = generation;
            advertise_tracker_ = tracker;

            // Counted before anything starts, so no status event can
            // complete the call early. A provider already advertising has
            // nothing left to report.
            std::size_t to_start = start_publisher ? 1 : 0;
            for (const auto& provider : providers)
            {
                if (provider.AdvertisementStatus() != WDBG::GattServiceProviderAdvertisementStatus::Started)
                    ++to_start;
            }
            tracker->remaining.store(to_start);

            // The publisher a failed start left behind goes first, with
            // its StatusChanged handler (R1-242).
            release_advertiser_noexcept();
            advertiser_ = make_advertisement_publisher(settings, data);

            const std::weak_ptr<SharedLeState> weak = le_;
            advertiser_status_token_ = watch_publisher_status(advertiser_, weak, tracker);

            if (start_publisher)
            {
                std::scoped_lock lock(tracker->mutex);
                tracker->publisher = advertiser_;
            }

            // Every provider is watched for the advertisement's life, also one
            // already advertising, which only the count above leaves out (R2-10).
            for (const auto& provider : providers)
            {
                const auto token = watch_provider_status(provider, weak, tracker);

                std::scoped_lock lock(tracker->mutex);
                tracker->providers.emplace_back(provider, token);
            }

            if (start_publisher)
                advertiser_.Start();

            for (const auto& uuid : le_->advertise_service_uuids)
                start_service_advertising_locked(*le_, le_->gatt_services[uuid]);
            services_lock.unlock();

            // Every provider was already advertising and there is no
            // publisher: nothing will report, so the start is done now.
            if (to_start == 0)
                finish_advertise_start(le_, tracker, Error::Ok, {});

            message.clear();
            return Error::Ok;
        }
        catch (const winrt::hresult_error& error)
        {
            if (services_lock.owns_lock())
                services_lock.unlock();

            // The core drops the op when this call fails; nothing may
            // complete it later.
            if (advertise_tracker_)
            {
                advertise_tracker_->done.store(true);
                revoke_advertise_handlers(advertise_tracker_);
                advertise_tracker_.reset();
            }
            le_->le_advertising.store(false);
            release_advertiser_noexcept();
            message = winrt::to_string(error.message());
            return Error::OperationFailed;
        }
    }

    // Reports nothing: the generation moves on before anything stops, so
    // the status events the stops cause are stale (R2-10).
    Error WindowsBackend::le_advertise_stop(std::string& message)
    {
        {
            std::scoped_lock flag_lock(le_->advertise_flag_mutex);
            le_->le_advertising.store(false);
            ++le_->advertise_generation;
        }

        if (advertise_tracker_)
        {
            finish_advertise_start(
                le_,
                advertise_tracker_,
                Error::OperationFailed,
                "Advertising stopped before it started");
            revoke_advertise_handlers(advertise_tracker_);
            advertise_tracker_.reset();
        }

        try
        {
            {
                std::scoped_lock lock(le_->services_mutex);
                for (auto& [_, service] : le_->gatt_services)
                {
                    if (service && service->provider)
                        service->provider.StopAdvertising();
                }
            }
            release_advertiser_noexcept();
            message.clear();
            return Error::Ok;
        }
        catch (const winrt::hresult_error& error)
        {
            release_advertiser_noexcept();
            message = winrt::to_string(error.message());
            return Error::OperationFailed;
        }
    }

    bool WindowsBackend::le_advertise_is_running() const
    {
        return le_->le_advertising.load();
    }

    Error WindowsBackend::le_server_start(std::string& message)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }
        if (!supports_le_server())
        {
            message = "BLE GATT server mode is not supported by this Windows adapter";
            return Error::NotSupported;
        }

        le_server_open_.store(true);
        message.clear();
        return Error::Ok;
    }

    // Ends advertising too (R2-20): Windows publishes a service only while
    // its provider advertises, so the two cannot outlive each other.
    Error WindowsBackend::le_server_stop(std::string& message)
    {
        std::string ignored;
        le_advertise_stop(ignored);

        le_server_open_.store(false);
        clear_queued_notifies(notify_queue_);
        complete_parked_gatt_requests_noexcept();
        clear_gatt_services_noexcept();
        release_server_sessions_noexcept(le_);
        message.clear();
        return Error::Ok;
    }

    bool WindowsBackend::le_server_is_running() const
    {
        return le_server_open_.load();
    }

    // What the central's session negotiated (R2-32): the key is its
    // session's device id.
    std::int32_t WindowsBackend::le_server_connection_mtu(const std::string& central) const
    {
        WDBG::GattSession session{nullptr};
        {
            std::scoped_lock lock(le_->server_sessions_mutex);
            const auto it = le_->server_sessions.find(central);
            if (it != le_->server_sessions.end())
                session = it->second.session;
        }

        try
        {
            if (session)
                return static_cast<std::int32_t>(session.MaxPduSize());
        }
        catch (const winrt::hresult_error&)
        {
        }
        return 23;
    }

    Error WindowsBackend::le_server_add_service(std::uint64_t op_id, const LeServerService& service, std::string& message)
    {
        if (!le_server_open_.load())
        {
            message = "The LE server is not running; call bluetooth_le_server_start first";
            return Error::OperationFailed;
        }

        LocalServiceDefinition definition;
        const Error parsed = to_local_service(service, definition, message);
        if (parsed != Error::Ok)
            return parsed;

        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(le_->services_mutex);
            if (le_->gatt_services.find(definition.uuid) != le_->gatt_services.end() ||
                le_->adding_services.find(definition.uuid) != le_->adding_services.end())
            {
                message = "BLE service already exists";
                return Error::Busy;
            }
            le_->adding_services.insert(definition.uuid);
            generation = le_->server_generation;
        }

        // Creating the provider and each attribute waits on WinRT, so it
        // runs on the worker, which completes op_id (R1-103).
        const auto le = le_;
        const bool posted = le_worker_ && le_worker_->post(
            [le, op_id, definition, generation]()
            {
                build_local_service(le, op_id, definition, generation);
            });

        if (!posted)
        {
            std::scoped_lock lock(le_->services_mutex);
            le_->adding_services.erase(definition.uuid);
            message = "The Bluetooth worker thread is not running";
            return Error::OperationFailed;
        }

        message.clear();
        return Error::Ok;
    }

    Error WindowsBackend::le_server_clear_services(std::string& message)
    {
        clear_queued_notifies(notify_queue_);
        complete_parked_gatt_requests_noexcept();
        clear_gatt_services_noexcept();
        message.clear();
        return Error::Ok;
    }

    Error WindowsBackend::le_server_respond_read(
        std::int32_t request_id,
        std::int32_t status,
        const std::vector<std::uint8_t>& value,
        std::string& message)
    {
        return respond_parked(*le_, le_->pending_gatt_reads, request_id, "read",
            [request_id, status, &value](PendingGattRead& pending)
            {
                // status is a BluetoothAttError the core validated: the ATT
                // code itself. The value is read only for a success.
                GMBT_TRACE(
                    "Responding to GATT read: request_id=%d status=%d request_state=%d request_offset=%u bytes=%zu",
                    request_id,
                    status,
                    static_cast<int>(pending.request.State()),
                    pending.request.Offset(),
                    status == 0 ? value.size() : std::size_t{0});

                if (status == 0)
                    pending.request.RespondWithValue(bytes_to_buffer(value));
                else
                    pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(status));
            },
            message);
    }

    Error WindowsBackend::le_server_respond_write(
        std::int32_t request_id,
        std::int32_t status,
        std::string& message)
    {
        return respond_parked(*le_, le_->pending_gatt_writes, request_id, "write",
            [request_id, status](PendingGattWrite& pending)
            {
                GMBT_TRACE(
                    "Responding to GATT write: request_id=%d status=%d with_response=%d request_state=%d bytes=%u",
                    request_id,
                    status,
                    pending.with_response ? 1 : 0,
                    static_cast<int>(pending.request.State()),
                    pending.request.Value() ? pending.request.Value().Length() : 0);

                if (pending.with_response)
                {
                    if (status == 0)
                        pending.request.Respond();
                    else
                        pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(status));
                }
            },
            message);
    }

    Error WindowsBackend::le_server_notify_value(
        std::uint64_t characteristic_id,
        const std::string& central,
        const std::vector<std::uint8_t>& value,
        std::string& message)
    {
        WDBG::GattLocalCharacteristic characteristic{nullptr};
        {
            std::scoped_lock lock(le_->services_mutex);
            for (const auto& [_, service] : le_->gatt_services)
            {
                if (!service)
                    continue;
                const auto it = service->characteristics.find(characteristic_id);
                if (it != service->characteristics.end() && it->second && it->second->characteristic)
                {
                    characteristic = it->second->characteristic;
                    break;
                }
            }
        }
        if (!characteristic)
        {
            message = "No GATT server characteristic has this id";
            return Error::NotFound;
        }

        // A notify to one central goes to its subscription only (R1-48).
        WDBG::GattSubscribedClient client{nullptr};
        if (!central.empty())
        {
            try
            {
                for (const auto& subscribed : characteristic.SubscribedClients())
                {
                    if (winrt::to_string(subscribed.Session().DeviceId().Id()) == central)
                    {
                        client = subscribed;
                        break;
                    }
                }
            }
            catch (const winrt::hresult_error& error)
            {
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }

            if (!client)
            {
                message = "The central is not subscribed to this characteristic";
                return Error::NotFound;
            }
        }

        const auto notifies = notify_queue_;
        bool start = false;
        {
            std::scoped_lock lock(notifies->mutex);
            if (notifies->queue.size() >= k_max_queued_notifies)
            {
                message = "Too many GATT notifications are waiting to be sent";
                return Error::Busy;
            }

            PendingNotify pending;
            pending.characteristic = characteristic;
            pending.client = client;
            pending.bytes = value;
            notifies->queue.push_back(std::move(pending));

            start = !notifies->in_flight;
            if (start)
                notifies->in_flight = true;
        }

        if (start)
            start_next_notify(notifies);

        message.clear();
        return Error::Ok;
    }

    // Completes every request still parked, so no central waits out the ATT
    // timeout on a server that stopped or dropped its services. The core
    // answers the ones it holds first; this catches anything left.
    void WindowsBackend::complete_parked_gatt_requests_noexcept() noexcept
    {
        std::unordered_map<std::int32_t, PendingGattRead> reads;
        std::unordered_map<std::int32_t, PendingGattWrite> writes;
        {
            std::scoped_lock lock(le_->gatt_request_mutex);
            reads.swap(le_->pending_gatt_reads);
            writes.swap(le_->pending_gatt_writes);
        }

        for (auto& [_, pending] : reads)
            refuse_parked_noexcept(pending, true);
        for (auto& [_, pending] : writes)
            refuse_parked_noexcept(pending, pending.with_response);
    }

    // Drops every service with its handlers revoked; an add still in
    // flight sees the new generation and is dropped too.
    void WindowsBackend::clear_gatt_services_noexcept() noexcept
    {
        std::unordered_map<std::string, std::shared_ptr<LocalGattServiceState>> services;
        {
            std::scoped_lock lock(le_->services_mutex);
            services.swap(le_->gatt_services);
            ++le_->server_generation;
        }

        for (auto& [_, service] : services)
        {
            if (service)
                release_local_service_noexcept(*service);
        }
    }

    void WindowsBackend::release_advertiser_noexcept() noexcept
    {
        try
        {
            if (advertiser_)
            {
                if (advertiser_status_token_.value != 0)
                {
                    advertiser_.StatusChanged(advertiser_status_token_);
                    advertiser_status_token_ = {};
                }
                advertiser_.Stop();
                advertiser_ = nullptr;
            }
        }
        catch (...)
        {
            advertiser_ = nullptr;
            advertiser_status_token_ = {};
        }
    }
}

#endif
