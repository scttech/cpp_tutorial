// A demo of your logger (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>

#include "logging.hpp"

int main() {
    MemorySink plain;      // outlives the sinks that point at it: declared first
    MemorySink delivered;
    Logger logger;

    logger.emplace<MemorySink>();                       // sink 1: keeps "[LEVEL] message" lines
    JsonSink& json = logger.emplace<JsonSink>();        // sink 2: keeps JSON lines (a JsonSink& comes back: the real type)
    logger.emplace<FilterSink>(plain, Level::Warn);     // sink 3: forwards only warnings and errors to `plain`
    BufferedSink& buffered = logger.emplace<BufferedSink>(delivered);  // sink 4: holds everything back until flushed

    logger.info("service started");
    logger.warn("disk is \"low\"");
    logger.error("out of memory");

    std::cout << "json sink:\n";
    for (const std::string& line : json.lines()) std::cout << "  " << line << "\n";
    std::cout << "filtered into `plain`:\n";
    for (const std::string& line : plain.lines()) std::cout << "  " << line << "\n";
    std::cout << "buffered, not yet delivered: " << buffered.pending() << " message(s), target has " << delivered.lines().size() << " lines\n";

    std::cout << "flush_all flushed " << logger.flush_all() << " sink(s); the target now has " << delivered.lines().size() << " lines:\n";
    for (const std::string& line : delivered.lines()) std::cout << "  " << line << "\n";
}
