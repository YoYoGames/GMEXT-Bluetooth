################################################################################
# ExtensionCore
################################################################################

ExtensionCore is the shared runtime foundation required by all GameMaker
extensions. It must be included as a dependency in every extension that uses
the standard GM wire protocol for communicating between GML and native code.

It provides:
  - A binary wire protocol for marshalling GML values (arrays, structs,
    primitives, pointers) into buffers that can be passed to and from native
    extension functions.
  - Managed global buffers for arguments, return values, and async callbacks.
  - A GML-side function dispatcher that allows native (C++) code to call GML
    functions asynchronously, with automatic lifecycle management.
  - A Java (Android) implementation of the same wire protocol (GMExtWire),
    including async dispatch queues and GML function reference wrappers.
  - A utility class (GMExtUtils) to retrieve extension options from the
    GameMaker runner on Android via JNI.

--------------------------------------------------------------------------------
WIRE PROTOCOL
--------------------------------------------------------------------------------

Values are encoded as <type_tag><payload> pairs using little-endian byte order.

GML type tag macros:

  EXT_CORE_GM_TYPE_STRUCT        (255)  - GML struct (key-value pairs)
  EXT_CORE_GM_TYPE_ARRAY         (254)  - heterogeneous array
  EXT_CORE_GM_TYPE_BUFFER        (253)  - raw buffer
  EXT_CORE_GM_TYPE_POINTER       (252)  - pointer (64-bit)
  EXT_CORE_GM_TYPE_UNDEFINED     (251)  - undefined value
  EXT_CORE_GM_TYPE_TYPED_ARRAY   (250)  - homogeneous typed array
  EXT_CORE_GM_TYPE_TYPED_STRUCT  (249)  - typed struct (identified by CODEC_ID)

Scalar types reuse standard GameMaker buffer_* type constants (e.g. buffer_s32,
buffer_f64, buffer_string, buffer_bool, buffer_u64).

Strings are encoded as: <buffer_string tag> <u32 byte_length> <utf8 bytes + NUL>

--------------------------------------------------------------------------------
GML API (ExtensionCore_api.gml)
--------------------------------------------------------------------------------

All functions are internal and prefixed with __ext_core_ or __GMNative.
They are not intended to be called directly by game code.

  __ext_core_get_args_buffer([_request_size])
      Returns the global args buffer, seeked to the start and cleared.
      Extensions write marshalled arguments into this before calling native
      functions.

  __ext_core_get_ret_buffer([_request_size])
      Returns the global return buffer (default 8192 bytes, grow-able).
      Native functions write return values into this buffer.

  __ext_core_get_async_buffer([_request_size])
      Returns the global async buffer used for native-to-GML callbacks.

  __ext_core_buffer_marshal_value(_buffer, _value)
      Encodes any GML value into the buffer using the wire protocol.
      Supports: arrays, structs, pointers, strings, bools, int32, int64,
      real (f64), and undefined.

  __ext_core_buffer_unmarshal_value(_buff, _decoders)
      Reads and decodes one value from the buffer. Typed structs are decoded
      using the _decoders array, where each entry is a function keyed by
      CODEC_ID.

  __ext_core_function_map()
      Returns the global ds_map that holds live GML function references
      registered for native access. Keyed by int64 pointer handle.

  __ext_core_function_register(_callable, _dispatcher)
      Registers a GML function so native code can call it. Reference-counted:
      the same function can be registered multiple times. Kicks the dispatcher
      to ensure polling is active. Returns the handle (int64).

  __ext_core_function_dispatch_calls(_handler, _decoders)
      Polls the native side for pending calls and releases. Called once per
      frame by the dispatcher's time source. Returns the number of released
      function references.

  __GMNativeFunctionDispatcher(_handler, _decoders) [constructor]
      Manages the frame-by-frame polling loop for GML functions exposed to
      native code. Uses a time_source that runs every frame while there are
      pending references, and pauses itself when the reference count drops
      to zero. Call .dispatch([_amount]) when registering new functions.

--------------------------------------------------------------------------------
ANDROID (Java) API
--------------------------------------------------------------------------------

GMExtWire.java
--------------
Implements the full wire protocol on the Java/Android side, mirroring the GML
implementation. All encoding uses little-endian byte order.

  ValueType (enum)
      Maps Java types to their byte tags in the wire protocol. Matches the
      GML macros above.

  GMKind (enum)
      Semantic kind of a decoded value: NULL, BOOL, BYTE, SHORT, INT, LONG,
      FLOAT, DOUBLE, STRING, ARRAY, OBJECT, TYPED_STRUCT, TYPED_ARRAY,
      BUFFER, POINTER.

  ITypedStruct (interface)
      Implement this interface for structs that need typed encoding. The
      implementing class must define:
          public static final int CODEC_ID;
      This is used by the reflection-based codec resolution system and must
      be kept by ProGuard (rules are injected automatically by ExtensionCore).

  DataStream
      A growable ByteBuffer wrapper with typed put() methods. Supports
      byte, short, int, long, float, double, boolean, String, ITypedStruct,
      Optional<?>, and List<?>. Use putRaw*() for untagged writes.

  ArrayStream / StructStream
      Builders for GM Array and GM Struct wire values. Use .add(v) and
      .kv(key, value) respectively. Call .writeTo(ByteBuffer) to flush.

  TypedArrayStream<T>
      Builder for homogeneous typed arrays. Supported element types:
      ITypedStruct subclasses, Integer, Long, String.

  GMValue
      Discriminated union representing a decoded GM value. Access the kind
      with .kind() and the payload with the appropriate accessor
      (asBool, asInt, asLong, asFloat, asDouble, asString, asArray, asObject).

  GMDispatcher (interface) / DispatchQueue
      Thread-safe, lock-free queue for native-to-GML async callbacks.
      Call .dispatch(DataStream) from any thread; call .fetch(ByteBuffer)
      from the GML polling function each frame.

  GMFunction
      Wraps a GML function reference (int64 handle) received from GML.
      Call .call(Object... args) to queue an invocation, .release() to
      explicitly free the reference. Automatically releases via Java Cleaner
      when garbage-collected.

  Read/write primitives:
      readI8 / writeI8, readI16 / writeI16, readI32 / writeI32,
      readI64 / writeI64, readF32 / writeF32, readF64 / writeF64,
      readBool / writeBool, readString / writeString.

  Collection helpers:
      readList / writeList       - length-prefixed (int32) lists
      readVector / writeVector   - fixed-count lists (count known externally)
      readFixedArray / writeFixedArray - fixed-count arrays
      readGMValue / readGMArray / readGMObject - decode full GM wire values
      readGMFunction             - decode a GML function reference

GMExtUtils.java
---------------
  GetExtensionOption(String extName, String optName) -> String
      Retrieves a named option for a given extension from the GameMaker runner
      via JNI (RunnerJNILib.extOptGetString).

--------------------------------------------------------------------------------
ANDROID BUILD REQUIREMENTS (injected automatically)
--------------------------------------------------------------------------------

  - Java source and target compatibility: VERSION_17
  - coreLibraryDesugaring enabled
  - Gradle dependency: com.android.tools:desugar_jdk_libs:2.0.4
  - ProGuard: ITypedStruct implementations and their CODEC_ID fields are kept.

--------------------------------------------------------------------------------
IOS BUILD REQUIREMENTS (injected automatically)
--------------------------------------------------------------------------------

  - SWIFT_OBJC_INTEROP_MODE = objcxx
  - LD_RUNPATH_SEARCH_PATHS configured for @executable_path/Frameworks and
    @loader_path/Frameworks

--------------------------------------------------------------------------------
LICENSE
--------------------------------------------------------------------------------

Use of GameMaker and its Source Code are governed by the Opera Terms of Service
and the GameMaker Source Code License Agreement.
See: https://gamemaker.io/legal/sourcecode
