/*
 * Copyright (c) 2024 Rory Walsh
 *
 * Lattice is licensed under the MIT License. See the LICENSE file for details.
 * This software is provided "as-is", without any express or implied warranty.
 * See the LICENSE file for more details.
 */

#pragma once

#include "httplib.h"
namespace lattice {


class Server
{
  public:
    Server();
    ~Server() { stop(); }
    void changeMountPoint(std::string mp);

    void start(std::string mountPoint);

    bool isRunning() { return isThreadRunning(); }
    httplib::Server &getServer() { return mServer; }

    int getCurrentPort() { return mPortNumber; }

    bool isThreadRunning() { return serverThread.joinable(); }

  protected:
    void run();
    void stop();

  private:
    httplib::Server mServer;
    int mPortNumber;
    bool isListening = true;
    std::string mountPoint = "";
    std::thread serverThread;
};

}