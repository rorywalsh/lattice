#include <nlohmann/json.hpp>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <mutex>
#include "../LatticeMemoryQueue.h"
#include <gui/choc_WebView.h>

class Timer
{
public:
    using Callback = std::function<void()>;
    
    Timer(int intervalMs, Callback callback) 
        : interval(intervalMs), cb(callback), running(false) {}
    
    ~Timer() { stop(); }
    
    void start() {
        if (running) return;
        running = true;
        thread = std::thread([this] {
            while (running) {
                auto start = std::chrono::steady_clock::now();
                cb();
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                auto sleepTime = interval - elapsed;
                if (sleepTime > 0) {
                    std::unique_lock<std::mutex> lock(mutex);
                    cv.wait_for(lock, std::chrono::milliseconds(sleepTime), [this] { return !running; });
                }
            }
        });
    }
    
    void stop() {
        if (!running) return;
        running = false;
        cv.notify_all();
        if (thread.joinable()) {
            thread.join();
        }
    }
    
private:
    int interval;
    Callback cb;
    bool running;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
};

class WebViewApp
{
public:
    WebViewApp(int argc, char *argv[])
    {
        // Parse command-line arguments
        if (argc != 10)
        {
            std::cerr << "Not enough args:" << argc << " Usage: " << argv[0] << " <x11WindowId> <x> <y> <width> "
                      << "<height> <scale> <isTransparent>\n";
            exit(1);
        }

        // Extract arguments
        x11WindowId = (Window)atol(argv[1]);

        std::cout << "name of shared memory map in child:" << std::string(argv[2]) << std::endl;

        try
        {
            memoryQueue = std::make_unique<lattice::SharedMemoryQueue>(std::string(argv[2]), 100, 1024);
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

        logMessage("Plugin X11 Window ID: " + std::to_string(x11WindowId));

        createWebView();
        webView->navigate("https://www.example.com");

        // Start timer with 10ms interval
        timer = std::make_unique<Timer>(10, [this] { readFromQueue(); });
        timer->start();
    }

    ~WebViewApp() 
    { 
        logMessage("Destructor");
        if (timer) {
            timer->stop();
        }
    }

    void run() const
    {
        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            logMessage("Failed to open X11 display");
            return;
        }

        while (!shouldExit)
        {
            // Process X11 events
            while (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                // Handle any specific X11 events if needed
            }

            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        XCloseDisplay(display);
    }

private:
    std::unique_ptr<choc::ui::WebView> webView;
    std::unique_ptr<Timer> timer;
    Window x11WindowId;
    float x, y, width, height, scale;
    bool isTransparent;
    bool enableDevTools;
    bool shouldExit = false;
    std::mutex mutex;
    std::unique_ptr<lattice::SharedMemoryQueue> memoryQueue;

    static void logMessage(std::string_view message) { std::cout << "WebViewProc:" << message << std::endl; }

    void createWebView()
    {
        logMessage("Creating webview widget");

        choc::ui::WebView::Options options;
        options.enableDebugMode = enableDevTools;
        options.transparentBackground = isTransparent;

        webView = std::make_unique<choc::ui::WebView>(options);


        // Get the native window handle from the WebView
        void* nativeHandle = webView->getViewHandle();
        if (!nativeHandle) {
            logMessage("Failed to get WebView native handle");
            return;
        }

        // Reparent the WebView to our target X11 window
        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            logMessage("Failed to open X11 display");
            return;
        }

        Window webViewWindow = reinterpret_cast<Window>(nativeHandle);
        XReparentWindow(display, webViewWindow, x11WindowId, 0, 0);

        // Set the new window's size and position
        XResizeWindow(display, webViewWindow, (int)width, (int)height);
        XMoveWindow(display, webViewWindow, (int)x, (int)y);

        // Make sure the changes take effect
        XFlush(display);
        XCloseDisplay(display);

        // webView->bind("IPlugSendMsg", [this](const choc::value::ValueView& args) {
        //     if (args.isString()) {
        //         try {
        //             auto message = nlohmann::json::parse(args.getString());
        //             memoryQueue->sendToHost(message);
        //         } catch (const std::exception& e) {
        //             logMessage("Error parsing JSON: " + std::string(e.what()));
        //         }
        //     }
        // });
    }

    void readFromQueue()
    {
        nlohmann::json message;
        std::unique_lock<std::mutex> lock(mutex);

        while (memoryQueue->receiveFromHost(message))
        {
            try {
                std::string msgData = message["data"];
                std::string command = message["command"];

                if (command == "LoadUrl")
                {
                    webView->navigate(msgData);
                }
                else if (command == "EvaluateJS")
                {
                    webView->evaluateJavascript(msgData);
                }
                else if (command == "Exit")
                {
                    shouldExit = true;
                }
            } catch (const std::exception& e) {
                logMessage("Error processing message: " + std::string(e.what()));
            }
        }
    }
};

int main(int argc, char *argv[])
{
    WebViewApp app(argc, argv);
    app.run();
    return 0;
}