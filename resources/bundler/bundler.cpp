// bundler.cpp

#include <archive.h>
#include <archive_entry.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef RUN_APP_NAME
#error Define RUN_APP_NAME, for example -DRUN_APP_NAME=\"myapp.exe\"
#define RUN_APP_NAME "app.exe"
#endif

using fpath = std::filesystem::path;

extern "C" {
    extern const unsigned char _binary_package_tar_xz_start[];
    extern const unsigned char _binary_package_tar_xz_end[];
}

static fpath exe_path()
{
    #ifdef _WIN32
    std::vector<wchar_t> buf(32768);
    const DWORD len = GetModuleFileNameW(nullptr, buf.data(),
                                         static_cast<DWORD>(buf.size()));
    if (len == 0 || len >= buf.size())
        throw std::runtime_error("GetModuleFileNameW failed");
    return fpath(std::wstring(buf.data(), len));
    #elif defined(__linux__)
    return std::filesystem::read_symlink("/proc/self/exe");
    #else
    // Replace with the platform's executable-path API if needed.
    return std::filesystem::current_path() / RUN_APP_NAME;
    #endif
}

static fpath make_temp_dir(const fpath& parent)
{
    // Try several unique names; never delete an existing path we didn't create.
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto ticks =
        std::chrono::steady_clock::now().time_since_epoch().count();

        fpath path = parent /
        (".bundler-temp-" + std::to_string(ticks) + "-" +
        std::to_string(attempt));

        std::error_code ec;
        if (std::filesystem::create_directory(path, ec))
            return path;

        if (ec && ec != std::errc::file_exists)
            throw std::system_error(ec, "Could not create temporary directory");
    }

    throw std::runtime_error("Could not create a unique temporary directory");
}

static bool safe_relative_path(const fpath& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() ||
        path.has_root_directory())
        return false;

    for (const auto& part : path) {
        if (part == "..")
            return false;
    }

    return true;
}

// #define TRUNCATE_TLD

static fpath remove_tld_path(const fpath& p) {
    #ifdef TRUNCATE_TLD
    auto it = p.begin();
    if (it == p.end())
        return {};

    fpath result;
    for (++it; it != p.end(); ++it)
        result /= *it;

    return result;
#else
    return p;
#endif
}

static void extract_archive(const fpath& destination,
                            const unsigned char* data,
                            std::size_t size)
{
    archive* reader = archive_read_new();
    if (!reader)
        throw std::runtime_error("Could not create archive reader");

    archive_read_support_filter_all(reader);
    archive_read_support_format_tar(reader);

    if (archive_read_open_memory(reader, data, size) != ARCHIVE_OK) {
        const char* message = archive_error_string(reader);
        std::string error = message ? message : "unknown archive error";
        archive_read_free(reader);
        throw std::runtime_error("Could not open embedded archive: " + error);
    }

    auto fail = [&](const std::string& message) {
        archive_read_free(reader);
        throw std::runtime_error(message);
    };

    archive_entry* entry = nullptr;
    int status = ARCHIVE_OK;

    while ((status = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);
        if (!name)
            fail("Archive entry has no pathname");

        fpath relative = fpath(name).lexically_normal();
        if (!safe_relative_path(relative))
            fail("Unsafe path in embedded archive");

        const fpath output = destination / remove_tld_path(relative);
        const auto type = archive_entry_filetype(entry);

        if (type == AE_IFDIR) {
            std::error_code ec;
            std::filesystem::create_directories(output, ec);
            if (ec)
                fail("Could not create extracted directory: " + output.string());

            archive_read_data_skip(reader);
            continue;
        }

        if (type != AE_IFREG) {
            // Do not extract symlinks, hardlinks, devices, or other special files.
            archive_read_data_skip(reader);
            continue;
        }

        std::error_code ec;
        std::filesystem::create_directories(output.parent_path(), ec);
        if (ec)
            fail("Could not create parent directory: " +
            output.parent_path().string());

        FILE* file = nullptr;
        #ifdef _WIN32
        file = _wfopen(output.c_str(), L"wb");
        #else
        file = std::fopen(output.c_str(), "wb");
        #endif
        if (!file)
            fail("Could not create extracted file: " + output.string());

        unsigned char buffer[16 * 1024];
        la_ssize_t count = 0;

        while ((count = archive_read_data(reader, buffer, sizeof buffer)) > 0) {
            if (std::fwrite(buffer, 1, static_cast<std::size_t>(count), file) !=
                static_cast<std::size_t>(count)) {
                std::fclose(file);
            fail("Error writing extracted file: " + output.string());
                }
        }

        const bool write_ok = std::fclose(file) == 0;
        if (count < 0 || !write_ok) {
            const char* message = archive_error_string(reader);
            fail(std::string("Error extracting file: ") +
            (message ? message : output.string()));
        }

        #ifndef _WIN32
        std::filesystem::permissions(
            output,
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add
        );
        #endif
    }

    if (status != ARCHIVE_EOF) {
        const char* message = archive_error_string(reader);
        fail(std::string("Error reading archive: ") +
        (message ? message : "unknown archive error"));
    }

    archive_read_free(reader);
}

static int run_app(const fpath& executable, const fpath& working_dir)
{
    #ifdef _WIN32
    #include <system_error>

    std::wstring command = L"\"" + executable.wstring() + L"\"";
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
        executable.c_str(),
                        mutable_command.data(),
                        nullptr, nullptr, FALSE, 0, nullptr,
                        working_dir.c_str(),
                        &startup, &process)) {
        const DWORD error = GetLastError();
    throw std::system_error(
        static_cast<int>(error),
                            std::system_category(),
                            "CreateProcessW failed for " + executable.string());
                        }

                        const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
                        if (wait_result != WAIT_OBJECT_0) {
                            const DWORD error =
                            (wait_result == WAIT_FAILED) ? GetLastError() : ERROR_GEN_FAILURE;
                            CloseHandle(process.hThread);
                            CloseHandle(process.hProcess);
                            throw std::system_error(
                                static_cast<int>(error),
                                                    std::system_category(),
                                                    "Waiting for child process failed");
                        }

                        DWORD exit_code = 0;
                        if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
                            const DWORD error = GetLastError();
                            CloseHandle(process.hThread);
                            CloseHandle(process.hProcess);
                            throw std::system_error(
                                static_cast<int>(error),
                                                    std::system_category(),
                                                    "Could not retrieve child process exit code");
                        }

                        CloseHandle(process.hThread);
                        CloseHandle(process.hProcess);
                        return static_cast<int>(exit_code);
                        #else

    const pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error(std::string("fork failed: ") +
        std::strerror(errno));

    if (pid == 0) {
        if (chdir(working_dir.c_str()) != 0)
            _exit(126);

        execl(executable.c_str(), executable.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        throw std::runtime_error(std::string("waitpid failed: ") +
        std::strerror(errno));
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
    #endif
}

int main()
{
    fpath temp_dir;
    int app_exit_code = 1;

    std::cout << "Extracting app bundle\n";
    try {
        const fpath executable_path = exe_path();
        temp_dir = make_temp_dir(executable_path.parent_path());

        #ifdef _WIN32
        SetFileAttributesW(temp_dir.c_str(), FILE_ATTRIBUTE_HIDDEN);
        #endif
        std::cout << "Temp dir created\n";

        const auto* data = _binary_package_tar_xz_start;
        const std::size_t size = static_cast<std::size_t>(
            _binary_package_tar_xz_end - _binary_package_tar_xz_start);

        extract_archive(temp_dir, data, size);
        std::cout << "Archive extracted\n";

        const fpath app_path = temp_dir / RUN_APP_NAME;
        std::cout << "RUN_APP_NAME=" RUN_APP_NAME " app_path=" << app_path << "\n";
        if (!std::filesystem::is_regular_file(app_path))
            throw std::runtime_error("Application not found in archive: " +
            app_path.string());

        app_exit_code = run_app(app_path, temp_dir);
    } catch (const std::exception& e) {
        std::cerr << "Bundler error: " << e.what() << '\n';
        app_exit_code = 1;
    }

    std::cout << "Cleaning up temp directory\n";
    if (!temp_dir.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
        if (ec) {
            std::cerr << "Failed to clean up temporary directory: "
            << ec.message() << '\n';
            if (app_exit_code == 0)
                app_exit_code = 1;
        }
    }

    return app_exit_code;
}
