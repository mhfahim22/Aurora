#pragma once
#include <string>
#include <vector>

/* Package manager commands */
int run_package_command(const std::vector<std::string>& args);

/* Documentation generator */
int run_doc_generator(const std::string& source_path, const std::string& output_path);
