#include <nlohmann/json.hpp>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mutex>
#include <webkit2/webkit2.h>
#include "../LatticeMemoryQueue.h"

/**
 * WebView Process Application
 * ==========================
 * 
 * Purpose:
 * This application provides an embedded WebView component for use in lattice audio plugins,
 * allowing HTML/JavaScript-based UIs to run in a secure, isolated process.
 * 
 * What this does:
 * 1. Creates and manages a WebKitGTK WebView instance in a separate process (required for audio plugins on Linux)
 * 2. Handles two-way communication between the host application and web content
 * 3. Maintains proper window embedding via X11 (for Linux hosts)
 * 
 * Architecture:
 * - Runs as a child process of the main plugin
 * - Communicates via shared memory queue (lattice::SharedMemoryQueue)
 * - Can also be run in standalone test mode
 * 
 * Lifecycle:
 * 1. When a host tries to open a plugin it will pass an X11 window handle to the plugin process
 * 2. The plugin then launches this process with said window handle
 * 2. Process embeds WebView into provided X11 window
 * 3. Host and WebView communicate via message passing:
 *    - Host -> WebView: Load URLs/Execute JS
 *    - WebView -> Host: Callback messages via SendMsg()
 * 4. On shutdown:
 *    - Host sends "Shutdown" command
 *    - Process cleans up WebView resources
 *    - Process terminates
 * 
 * Message Protocol:
 * - All messages are JSON formatted
 * - Supported commands:
 *   "LoadUrl" - Loads a URL in the WebView
 *   "EvaluateJS" - Executes JavaScript code
 *   "Shutdown" - Initiates clean exit
 * 
 * 
 * Usage:
 * Embedded Mode:
 *   ./WebViewProcess <x11WindowId> <shm_name> <x> <y> <width> <height> <scale> <isTransparent> <enableDevTools>
 * 
 * Test Mode:
 *   ./WebViewProcess --test
 * 
 * Dependencies:
 * - WebKitGTK
 * - GTK3
 * - X11 libraries
 * - nlohmann/json
 * 
 * Notes:
 * - Designed for Linux hosts (X11-based)
 * - All GPU rendering occurs in this isolated process
 * - Crash of this process won't affect host application
 */

class WebViewApp
{
  public:
    WebViewApp(int argc, char *argv[], bool isStandaloneTest = false)
    {
        // Initialize GTK
        gtk_init(&argc, &argv);

        // Initialize X11-related members
        xdisplay = nullptr;
        gtkX11Window = 0;
        isEmbedded = false;

        
        if (isStandaloneTest) {
            // Standalone test mode - create our own window and controls
            createStandaloneWindow();
            enableDevTools = true;
            
            // Create dummy memory queue for testing
            try {
                memoryQueue = std::make_unique<lattice::SharedMemoryQueue>("test_queue", 100, 65536);
                std::cout << "Test shared memory created successfully." << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "Failed to create test shared memory: " << e.what() << std::endl;
            }
        } else {
            // Original embedded mode
            if (argc != 10)
            {
                g_printerr("Not enough args:%d Usage: %s <x11WindowId> <x> <y> <width> "
                           "<height> <scale> <isTransparent>\n",
                           argc, argv[0]);
                exit(1);
            }

            // Extract arguments
            x11WindowId = (Window)atol(argv[1]);

            std::cout << "name of shared memory map in child:" << std::string(argv[2]) << std::endl;

            try
            {
                memoryQueue = std::make_unique<lattice::SharedMemoryQueue>(std::string(argv[2]), 100, 65536);
                std::cout << "Shared memory created successfully." << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to create shared memory: " << e.what() << std::endl;
            }

            x = atof(argv[3]);
            y = atof(argv[4]);
            width = atof(argv[5]);
            height = atof(argv[6]);
            scale = atof(argv[7]);
            isTransparent = (strcmp(argv[8], "true") == 0);
            enableDevTools = (strcmp(argv[9], "true") == 0);

            // Debug: Print X11 window IDs
            logMessage("Plugin X11 Window ID: " + std::to_string(x11WindowId));

            // Create the GTK window
            createWindow();
        }

        // Create the WebView (same for both modes)
        createWebView(enableDevTools);

        // Set transparency if needed
        if (isTransparent)
        {
            setTransparency();
        }

        // Load a default webpage
        webkit_web_view_load_uri(webview, "https://www.example.com");

        // Show the window
        gtk_widget_show_all(window);
        
        // Connect the destroy signal to exit the application
        g_signal_connect(window, "destroy", G_CALLBACK(onDestroy), this);

        // Periodically check the named pipe for new JavaScript code
        g_timeout_add(10, readFromQueue, this);

        // Make sure our window gets properly destroyed
        g_signal_connect(window, "delete-event", G_CALLBACK(onDestroy), this);

        // Set up signal handlers for clean shutdown
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = handleSigterm;
        sigaction(SIGTERM, &action, NULL);
    }

    ~WebViewApp() 
    { 
        logMessage("Destructor");
        if (xdisplay) {
            XCloseDisplay(xdisplay);
        }
    }

    void run()
    {
        // Start the GTK main loop
        gtk_main();
    }

  private:
    char buffer[4096 * 256];
    GtkWidget *window;
    WebKitWebView *webview;
    Window x11WindowId;
    float x, y, width, height, scale;
    bool isTransparent;
    bool enableDevTools;
    bool isEmbedded;
    std::mutex mutex;
    std::unique_ptr<lattice::SharedMemoryQueue> memoryQueue;
    Display* xdisplay;
    Window gtkX11Window;

    static void logMessage(std::string_view message) { std::cout << "WebViewProc:" << message << std::endl; }

    void createStandaloneWindow()
    {
        logMessage("Creating standalone test window");
        
        // Create a regular GTK window (not a plug)
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "WebView Test Window");
        gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
        
        // Set up test controls
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        
        GtkWidget *urlEntry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(urlEntry), "Enter URL");
        GtkWidget *loadButton = gtk_button_new_with_label("Load");
        GtkWidget *testButton = gtk_button_new_with_label("Test JS");
        GtkWidget *transparentToggle = gtk_check_button_new_with_label("Transparent");
        
        g_signal_connect(loadButton, "clicked", G_CALLBACK(onLoadClicked), this);
        g_signal_connect(testButton, "clicked", G_CALLBACK(onTestClicked), this);
        g_signal_connect(transparentToggle, "toggled", G_CALLBACK(onTransparentToggled), this);
        g_object_set_data(G_OBJECT(loadButton), "url-entry", urlEntry);
        
        gtk_box_pack_start(GTK_BOX(controls), urlEntry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(controls), loadButton, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(controls), testButton, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(controls), transparentToggle, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), controls, FALSE, FALSE, 0);
        
        // WebView will be added in createWebView
        gtk_container_add(GTK_CONTAINER(window), box);
    }

    void createWindow()
    {
        logMessage("Creating window");

        // Create a GTK plug (embedded window) using the specified X11 window ID
        window = gtk_plug_new(x11WindowId);
        isEmbedded = true;

        // Set the default size of the window
        gtk_window_set_default_size(GTK_WINDOW(window), (int)width, (int)height);

        // Move the window to the specified position
        gtk_window_move(GTK_WINDOW(window), (int)x, (int)y);

        // Remove window decorations
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    }

    void createWebView(bool enableDebug)
    {
        logMessage("Creating webview widget");
        // Create a user content manager to inject scripts
        WebKitUserContentManager *userContentManager = webkit_user_content_manager_new();

        // Create the WebView with the content manager
        webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(userContentManager));

        // Define the JavaScript function to be injected.
        // sendMessageFromUI matches the interface expected by main.js (plugin context).
        // SendMsg is kept for any legacy callers.
        const char *js_code = "function SendMsg(m) {"
                              "   window.webkit.messageHandlers.callback.postMessage(m);"
                              "}"
                              "window.sendMessageFromUI = function(m) {"
                              "   window.webkit.messageHandlers.callback.postMessage(m);"
                              "};";

        // Create a WebKit user script that runs at document start
        WebKitUserScript *script =
            webkit_user_script_new(js_code,
                                   WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,        // Inject into top-level frame
                                   WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, // Run before page loads
                                   NULL, NULL);

        // Add the script to the WebView
        webkit_user_content_manager_add_script(userContentManager, script);

        // Register the message handler and connect it to a callback
        webkit_user_content_manager_register_script_message_handler(userContentManager, "callback");
        g_signal_connect(userContentManager, "script-message-received::callback", G_CALLBACK(onWebMessageReceived),
                         this);

        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

        // Add the WebView to the GTK window
        if (GTK_IS_BOX(gtk_bin_get_child(GTK_BIN(window)))) {
            GtkWidget *box = gtk_bin_get_child(GTK_BIN(window));
            gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(webview), TRUE, TRUE, 0);
        } else {
            gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        }

        // Always enable file access for loading local resources
        {
            WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
            webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
            webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);

            // Enable Developer Tools if requested
            if (enableDebug)
                webkit_settings_set_enable_developer_extras(settings, TRUE);
        }

        g_object_ref_sink(webview);  // Take ownership
    }

    static void onWebMessageReceived(WebKitUserContentManager *manager, WebKitJavascriptResult *result,
                                     gpointer user_data)
    {
        WebViewApp *app = static_cast<WebViewApp *>(user_data);
        JSCValue *value = webkit_javascript_result_get_js_value(result);

        // Print the raw value as JSON (for other types)
        gchar *json = jsc_value_to_json(value, 0);
        auto j = nlohmann::json::parse(json);

        app->memoryQueue->sendToHost(j);
        logMessage("Received web message: " + j.dump());

        g_free(json);
    }

    void setTransparency()
    {
        GdkScreen *screen = gtk_widget_get_screen(window);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

        if (visual && gdk_screen_is_composited(screen))
        {
            gtk_widget_set_visual(window, visual);
            gtk_widget_set_app_paintable(window, TRUE);
        }
    }

    static gboolean readFromQueue(gpointer data)
    {
        WebViewApp *app = static_cast<WebViewApp *>(data);

        nlohmann::json message;
        std::unique_lock<std::mutex> lock(app->mutex);

        while (app->memoryQueue->receiveFromHost(message))
        {
            logMessage("Received message from memory queue: " + message.dump(4));
            std::string msgData = message["data"];
            std::string command = message["command"];

            // Process the message based on the "command" field
            if (command == "LoadUrl")
            {
                logMessage("Loading URL: " + msgData);
                webkit_web_view_load_uri(app->webview, msgData.c_str());
            }
            else if (command == "EvaluateJS")
            {
                logMessage("Evaluating JavaScript: " + msgData);
                webkit_web_view_evaluate_javascript(app->webview, msgData.c_str(), -1, NULL, NULL, NULL, NULL, NULL);
            }
            else if (command == "Closed") {  // <-- NEW: Handle shutdown command
                logMessage("Received shutdown request");
                g_idle_add([](gpointer data) -> gboolean {
                    WebViewApp* app = static_cast<WebViewApp*>(data);
                    if (app->window) {
                        gtk_widget_destroy(app->window);  // Triggers onDestroy()
                    }
                    return FALSE;  // Run once
                }, app);
            }
        }

        return TRUE; // Continue listening for more data
    }

    static void handleSigterm(int signum) 
    {
        logMessage("Received SIGTERM, shutting down");
        g_idle_add([](gpointer data) -> gboolean {
            WebViewApp* app = static_cast<WebViewApp*>(data);
            if (app->window) {
                // Trigger the destroy signal (which calls onDestroy)
                gtk_widget_destroy(app->window);
            }
            return FALSE;
        }, nullptr);
    }

    static void onDestroy(GtkWidget* widget, gpointer data) 
    {
        WebViewApp* app = static_cast<WebViewApp*>(data);
        logMessage("Starting cleanup...");
    
        // Disconnect all signals first to prevent callbacks during destruction
        if (app->webview) {
            WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(app->webview);
            g_signal_handlers_disconnect_by_data(manager, app);
            g_signal_handlers_disconnect_by_data(app->webview, app);
        }
    
        // Explicitly destroy the WebView before its container
        if (app->webview) {
            GtkWidget* webviewWidget = GTK_WIDGET(app->webview);
            gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(webviewWidget)), webviewWidget);
            app->webview = nullptr;  // Avoid dangling pointer
        }
    
        // Clean up X11 resources (if embedded)
        if (app->isEmbedded && app->xdisplay) {
            XUnmapWindow(app->xdisplay, app->gtkX11Window);
            XSync(app->xdisplay, False);
        }
    
        logMessage("Cleanup complete, quitting GTK");
        gtk_main_quit();
    }

    // Standalone mode callbacks
    static void onLoadClicked(GtkWidget *widget, gpointer data)
    {
        WebViewApp *app = static_cast<WebViewApp*>(data);
        GtkWidget *urlEntry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "url-entry"));
        const gchar *url = gtk_entry_get_text(GTK_ENTRY(urlEntry));
        
        if (url && *url) {
            if (!g_str_has_prefix(url, "http://") && !g_str_has_prefix(url, "https://")) {
                gchar *full_url = g_strdup_printf("https://%s", url);
                webkit_web_view_load_uri(app->webview, full_url);
                g_free(full_url);
            } else {
                webkit_web_view_load_uri(app->webview, url);
            }
        }
    }

    static void onTestClicked(GtkWidget *widget, gpointer data)
    {
        WebViewApp *app = static_cast<WebViewApp*>(data);
        const char *test_js = "document.body.style.backgroundColor = 'lightblue';"
                             "document.title = 'Modified by test script';"
                             "SendMsg({command:'Test', data:'This is a test message'});";
        webkit_web_view_evaluate_javascript(app->webview, test_js, -1, NULL, NULL, NULL, NULL, NULL);
    }

    static void onTransparentToggled(GtkWidget *widget, gpointer data)
    {
        WebViewApp *app = static_cast<WebViewApp*>(data);
        app->isTransparent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
        if (app->isTransparent) {
            app->setTransparency();
        } else {
            // Reset to normal visual
            GdkScreen *screen = gtk_widget_get_screen(app->window);
            GdkVisual *visual = gdk_screen_get_system_visual(screen);
            gtk_widget_set_visual(app->window, visual);
            gtk_widget_set_app_paintable(app->window, FALSE);
        }
    }
};

int main(int argc, char *argv[])
{
    // Check if we're running in test mode
    bool isStandaloneTest = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            isStandaloneTest = true;
            break;
        }
    }
    
    WebViewApp app(argc, argv, isStandaloneTest);
    app.run();
    return 0;
}