/*
 * Copyright (c) 2024 Rory Walsh
 *
 * Lattice is licensed under the MIT License. See the LICENSE file for details.
 * This software is provided "as-is", without any express or implied warranty.
 * See the LICENSE file for more details.
 */

#include "LatticeServer.h"
#include "LatticeUtils.h"

std::string dump_headers(const httplib::Headers &headers)
{
    std::string s;
    char buf[BUFSIZ];

    for (const auto &x : headers)
    {
        snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
        s += buf;
    }

    return s;
}

std::string dump_multipart_files(const httplib::MultipartFormDataMap &files)
{
    std::string s;
    char buf[BUFSIZ];

    s += "--------------------------------\n";

    for (const auto &x : files)
    {
        const auto &name = x.first;
        const auto &file = x.second;

        snprintf(buf, sizeof(buf), "name: %s\n", name.c_str());
        s += buf;

        snprintf(buf, sizeof(buf), "filename: %s\n", file.filename.c_str());
        s += buf;

        snprintf(buf, sizeof(buf), "content type: %s\n", file.content_type.c_str());
        s += buf;

        snprintf(buf, sizeof(buf), "text length: %zu\n", file.content.size());
        s += buf;

        s += "----------------\n";
    }

    return s;
}

std::string log(const httplib::Request &req, const httplib::Response &res)
{
    std::string s;
    char buf[BUFSIZ];

    s += "\n================================\n";

    snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(), req.version.c_str(), req.path.c_str());
    s += buf;

    std::string query;
    for (auto it = req.params.begin(); it != req.params.end(); ++it)
    {
        const auto &x = *it;
        snprintf(buf, sizeof(buf), "%c%s=%s", (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
                 x.second.c_str());
        query += buf;
    }
    snprintf(buf, sizeof(buf), "%s\n", query.c_str());
    s += buf;

    s += dump_headers(req.headers);
    s += dump_multipart_files(req.files);

    s += "--------------------------------\n";

    snprintf(buf, sizeof(buf), "%d\n", res.status);
    s += buf;
    s += dump_headers(res.headers);

    return s;
}

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
