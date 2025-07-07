/*
 * Copyright (c) 2024 Rory Walsh
 *
 * Lattice is licensed under the MIT License. See the LICENSE file for details.
 * This software is provided "as-is", without any express or implied warranty.
 * See the LICENSE file for more details.
 */

#include "LatticeServer.h"
#include "LatticeUtils.h"


lattice::Server::Server()
{
    std::cout << "creating server";
}

void lattice::Server::stop()
{
    if (serverThread.joinable())
    {
        // Close the server
        mServer.stop();
        // Join the server thread
        serverThread.join();
    }
}

void lattice::Server::changeMountPoint(std::string mp)
{
    if (!mServer.set_mount_point("/", mp))
        std::cout << ("couldn't set up mount point");
}

void lattice::Server::start(std::string mp)
{
    mountPoint = mp;
    isListening = true;

    if(!lattice::File::exists(mountPoint))
        return;
    
    if (!mServer.set_mount_point("/", mountPoint))
        std::cout << ("couldn't set mount point");

    mServer.set_logger(
        [](const auto &/*req*/, const auto &/*res*/)
        {
            //		std::cout << log(req, res) << std::endl;
        });

    mServer.Get("/stop", [&](const auto & /*req*/, auto & /*res*/) { mServer.stop(); });

    mPortNumber = mServer.bind_to_any_port("127.0.0.1");

    serverThread = std::thread(&lattice::Server::run, this);
}

void lattice::Server::run()
{
    mServer.listen_after_bind();
}
