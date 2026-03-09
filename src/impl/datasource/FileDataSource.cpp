#include "datasource/FileDataSource.h"
#include <fstream>
#include <stdexcept>

std::vector<unsigned char> FileDataSource::getRawBytes() {
    std::ifstream file(filePath, std::ios::binary); //open in binary mode

    if (!file) throw std::runtime_error("Could not open file: " + filePath);
    //read file into buffer
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());

    return buffer;
}