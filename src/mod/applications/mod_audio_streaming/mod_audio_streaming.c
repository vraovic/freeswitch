/* 
 *
 * mod_audio_streaming.c -- Freeswitch module for streaming audio to Norwood audio docker server over websockets
 *
 */

#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

static int callback_my_protocol(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf("Connection established\n");
        // You might want to send a message to the server here.
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        // Handle incoming messages from the server.
        printf("Received data: %s\n", (const char *)in);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            // Example of sending a simple "Hello" message to the server.
            unsigned char buf[LWS_PRE + 20];
            unsigned char *p = &buf[LWS_PRE];
            size_t n = sprintf((char *)p, "Hello");
            lws_write(wsi, p, n, LWS_WRITE_TEXT);
        }
        break;

    case LWS_CALLBACK_CLOSED:
        printf("Connection closed\n");
        break;

    default:
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "my-protocol",
        callback_my_protocol,
        0,
        512, // Adjust according to your needs
    },
    { NULL, NULL, 0, 0 } // Terminator
};

int main() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "lws init failed\n");
        return -1;
    }

    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = context;
    ccinfo.address = "example.com";
    ccinfo.port = 80;
    ccinfo.path = "/";
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "origin";
    ccinfo.protocol = protocols[0].name;

    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (wsi == NULL) {
        fprintf(stderr, "Client connect failed\n");
        return -1;
    }

    // Main event loop
    while (1) {
        lws_service(context, 0 /* timeout_ms */);
    }

    lws_context_destroy(context);

    return 0;
}
