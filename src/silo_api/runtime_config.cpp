#include "silo_api/runtime_config.h"

#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace silo_api {

void RuntimeConfig::overwriteFromFile(const std::filesystem::path& config_path) {
   SPDLOG_INFO("Reading runtime config from {}", config_path.string());

   try {
      YAML::Node node = YAML::LoadFile(config_path.string());
      if (node[DATA_DIRECTORY_OPTION]) {
         SPDLOG_DEBUG(
            "Using dataDirectory passed via config file: {}",
            node[DATA_DIRECTORY_OPTION].as<std::string>()
         );
         data_directory = node[DATA_DIRECTORY_OPTION].as<std::string>();
      }
      if (node[MAX_CONNECTIONS_OPTION]) {
         SPDLOG_DEBUG(
            "Using maximum queued http connections passed via config file: {}",
            node[MAX_CONNECTIONS_OPTION].as<int32_t>()
         );
         max_connections = node[MAX_CONNECTIONS_OPTION].as<int32_t>();
      }
      if (node[PARALLEL_THREADS_OPTION]) {
         SPDLOG_DEBUG(
            "Using parallel threads for accepting http connections as passed via config file: {}",
            node[PARALLEL_THREADS_OPTION].as<int32_t>()
         );
         parallel_threads = node[PARALLEL_THREADS_OPTION].as<int32_t>();
      }
      if (node[PORT_OPTION]) {
         SPDLOG_DEBUG("Using port passed via config file: {}", node[PORT_OPTION].as<uint16_t>());
         port = node[PORT_OPTION].as<uint16_t>();
      }
   } catch (const YAML::Exception& e) {
      throw std::runtime_error(
         "Failed to read runtime config from " + config_path.string() + ": " + std::string(e.what())
      );
   }
}

void RuntimeConfig::overwriteFromCommandLineArguments(
   const Poco::Util::AbstractConfiguration& config
) {
   if (config.hasProperty(DATA_DIRECTORY_OPTION)) {
      SPDLOG_DEBUG(
         "Using dataDirectory passed via command line argument: {}",
         config.getString(DATA_DIRECTORY_OPTION)
      );
      data_directory = config.getString(DATA_DIRECTORY_OPTION);
   }
   if (config.hasProperty(MAX_CONNECTIONS_OPTION)) {
      SPDLOG_DEBUG(
         "Using maximum queued http connections passed via command line argument: {}",
         config.getInt(MAX_CONNECTIONS_OPTION)
      );
      max_connections = config.getInt(MAX_CONNECTIONS_OPTION);
   }
   if (config.hasProperty(PARALLEL_THREADS_OPTION)) {
      SPDLOG_DEBUG(
         "Using parallel threads for accepting http connections as passed via command line "
         "argument: {}",
         config.getInt(PARALLEL_THREADS_OPTION)
      );
      parallel_threads = config.getInt(PARALLEL_THREADS_OPTION);
   }
   if (config.hasProperty(PORT_OPTION)) {
      SPDLOG_DEBUG(
         "Using port passed via command line argument: {}", config.getString(PORT_OPTION)
      );
      port = config.getUInt(PORT_OPTION);
   }
}

}  // namespace silo_api
