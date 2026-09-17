// owner, row, characteristic and action arrive via the creation struct.

locked = false;
clicked = false;

switch (action)
{
    case "read":
        text = "READ";
        image_xscale = 1.35;
        break;

    case "write":
        text = "WRITE";
        image_xscale = 1.45;
        break;

    case "subscribe":
        text = "SUBSCRIBE";
        image_xscale = 2.15;
        break;

    default:
        text = string_upper(action);
        image_xscale = 1.5;
        break;
}

image_yscale = 0.8;
