#include "main.h"
#include "logger/Logger.h"
#include "util/ThreadPoolSingleton.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

// backtrace() / backtrace_symbols_fd() are Linux/macOS-only.
#if defined(__linux__) || defined(__APPLE__)
#  include <execinfo.h>
#endif

// ─── Globals ────────────────────────────────────────────────────────────────
// Set by signal handlers; volatile + atomic ensures visibility across threads.
static volatile std::sig_atomic_t g_shutdownRequested = 0;

// ─── Forward declarations ────────────────────────────────────────────────────
static void setupSignalHandlers();
static void onFatalSignal(int sig);
static void printUsage(const char *programName);
static bool parseArgs(int argc, char **argv);

/**
 * @brief Entry point for the Chista Asabru database proxy.
 *
 * Initialisation order:
 * 1. Parse command-line arguments.
 * 2. Install signal handlers.
 * 3. Start proxy, protocol, and API-gateway servers.
 * 4. Wait for a shutdown signal (SIGTERM / SIGINT).
 * 5. Perform graceful shutdown.
 */
int main(int argc, char **argv) {
    // Seed the PRNG (used for load-balancer strategies).
    // std::mt19937 is preferred for new code, but srand is kept here for
    // compatibility with the existing load-balancer implementation.
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (!parseArgs(argc, argv)) {
        return EXIT_FAILURE;
    }

    setupSignalHandlers();

    // ── Proxy servers ──────────────────────────────────────────────────────
    if (initProxyServers() < 0) {
        LOG_ERROR("Fatal: failed to initialise proxy servers — aborting.");
        return EXIT_FAILURE;
    }

    // ── Protocol servers ───────────────────────────────────────────────────
    if (initProtocolServers() < 0) {
        LOG_ERROR("Fatal: failed to initialise protocol servers — aborting.");
        return EXIT_FAILURE;
    }

    // ── API-gateway servers ────────────────────────────────────────────────
    if (initApiGatewayServers() < 0) {
        LOG_ERROR("Fatal: failed to initialise API-gateway servers — aborting.");
        return EXIT_FAILURE;
    }

    LOG_INFO("Chista Asabru proxy started successfully. Waiting for shutdown signal...");

    // Block the main thread until a shutdown signal arrives.
    // sigwait() is safer than pause() because it allows us to handle SIGTERM
    // from within the main thread while other threads continue serving.
    sigset_t waitSet;
    sigemptyset(&waitSet);
    sigaddset(&waitSet, SIGTERM);
    sigaddset(&waitSet, SIGINT);

    int receivedSig = 0;
    sigwait(&waitSet, &receivedSig);

    LOG_INFO("Received signal " + std::to_string(receivedSig) + " -- initiating graceful shutdown...");

    // TODO: call per-server stop() methods when they are added to the interface.

    LOG_INFO("Chista Asabru proxy shut down cleanly.");
    return EXIT_SUCCESS;
}

// ─── Signal-handler setup ────────────────────────────────────────────────────

static void setupSignalHandlers() {
    // Block SIGTERM and SIGINT so they are handled synchronously via sigwait().
    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGTERM);
    sigaddset(&blockSet, SIGINT);
    pthread_sigmask(SIG_BLOCK, &blockSet, nullptr);

    // Ignore SIGPIPE: a broken client pipe must not kill the proxy process.
    struct sigaction saPipe{};
    saPipe.sa_handler = SIG_IGN;
    sigemptyset(&saPipe.sa_mask);
    sigaction(SIGPIPE, &saPipe, nullptr);

    // Install a handler for unrecoverable signals (SIGSEGV, SIGBUS, SIGFPE).
    struct sigaction saFatal{};
    saFatal.sa_handler = onFatalSignal;
    sigemptyset(&saFatal.sa_mask);
    // SA_RESETHAND: revert to default after first delivery so we do not loop.
    saFatal.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &saFatal, nullptr);
    sigaction(SIGBUS,  &saFatal, nullptr);
    sigaction(SIGFPE,  &saFatal, nullptr);
    sigaction(SIGILL,  &saFatal, nullptr);
    sigaction(SIGABRT, &saFatal, nullptr);
}

/**
 * @brief Handler for unrecoverable signals.
 *
 * Prints a stack trace to stderr and re-raises the signal so the OS can
 * write a core dump.  This function must be async-signal-safe.
 */
static void onFatalSignal(int sig) {
    // async-signal-safe calls only beyond this point.
    const char prefix[] = "\n[FATAL] signal ";
    write(STDERR_FILENO, prefix, sizeof(prefix) - 1);

    // Write signal number as ASCII (no printf -- not async-signal-safe).
    char sigBuf[8];
    int n = sig;
    int pos = static_cast<int>(sizeof(sigBuf)) - 1;
    sigBuf[pos--] = '\n';
    do { sigBuf[pos--] = '0' + (n % 10); n /= 10; } while (n && pos >= 0);
    write(STDERR_FILENO, sigBuf + pos + 1,
          static_cast<std::size_t>(static_cast<int>(sizeof(sigBuf)) - 1 - pos));

#if defined(__linux__) || defined(__APPLE__)
    void *frames[32];
    int nFrames = backtrace(frames, 32);
    backtrace_symbols_fd(frames, nFrames, STDERR_FILENO);
#endif

    // Restore default and re-raise so the OS can generate a core dump.
    raise(sig);
}

// ─── Argument parsing ────────────────────────────────────────────────────────

static void printUsage(const char *programName) {
    std::cerr
        << "Usage: " << programName << " [options]\n"
        << "  (no options currently -- all configuration is via config.xml)\n"
        << "  --help  Show this message\n";
}

static bool parseArgs(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return false;
        }
        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(argv[0]);
        return false;
    }
    return true;
}
