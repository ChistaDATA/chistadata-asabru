#pragma once
/**
 * @file main.h
 * @brief Top-level includes and forward declarations for the Chista Asabru proxy.
 *
 * Keep this file lean: only include what is needed to compile main.cpp.
 * Heavy configuration types live in their own headers.
 */

// ─── Standard library ────────────────────────────────────────────────────────
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <list>
#include <map>
#include <pthread.h>
#include <string>
#include <thread>
#include <unistd.h>   // write(), STDERR_FILENO
#include <utility>

// ─── Server initialisation routines ─────────────────────────────────────────
// Defined in proxy_server.h / protocol_server.h / api_gateway_server.h
#include "proxy_server.h"
#include "protocol_server.h"
#include "api_gateway_server.h"
