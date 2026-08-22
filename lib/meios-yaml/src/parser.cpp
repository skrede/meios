#include "meios/yaml/parser.h"

#include "document_reader.h"

#include <memory>

namespace meios
{

std::shared_ptr<const yaml_parser_handle> make_yaml_parser()
{
    return std::make_shared<const yaml_parser_handle>(
        std::make_unique<detail::yaml_document_reader>());
}

}
