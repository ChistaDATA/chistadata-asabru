#include "PipelineFactory.h"

void PipelineFactory::updateLibs() {
    if (!std::getenv("PLUGINS_FOLDER_PATH")) {
        throw std::runtime_error("PLUGINS_FOLDER_PATH environment variable is missing");
    }
    LOG_INFO("Checking for new libraries :");
    std::string pluginsFolderPath = std::getenv("PLUGINS_FOLDER_PATH");

#if __APPLE__
    const std::string libGlob(pluginsFolderPath + "/*.dylib");
#else
    const std::string libGlob(pluginsFolderPath + "/*.so");
#endif

    std::vector<std::string> filenames = globothy(libGlob);

    size_t before = m_libNames.size();

    for (std::string plugin : filenames) {
        m_libNames.insert(plugin);
    }
    size_t after = m_libNames.size();

    if (after - before > 0) {
        LOG_INFO("Found " + std::to_string(after - before) + " new plugins");
    }
}
