#include "guard_prelude.h"

#include <string>
#include <string_view>

namespace meios::detail
{

namespace
{

std::string_view scope_reads()
{
    return "_ABSENT = object()\n"
           "\n"
           "class _ScopeReadFailed(Exception):\n"
           "    pass\n"
           "\n"
           "# The reference evaluates over its table as globals, so every name read at any depth of\n"
           "# the expression, a lambda or a generator body included, is looked up here, the scope\n"
           "# before the mathematics names and helpers and both before the builtins.\n"
           "class _ScopeGlobals(dict):\n"
           "    __slots__ = ('_read', '_fallback')\n"
           "\n"
           "    def __missing__(self, name):\n"
           "        found = self._read(name)\n"
           "        if found is not _ABSENT:\n"
           "            return found\n"
           "        if name in self._fallback:\n"
           "            return self._fallback[name]\n"
           "        raise KeyError(name)\n"
           "\n";
}

std::string_view scope_namespace_class()
{
    return "class _ScopeNamespace:\n"
           "    __slots__ = ('__meios_member__',)\n"
           "\n"
           "    def __getattr__(self, item):\n"
           "        return object.__getattribute__(self, '__meios_member__')(item)\n"
           "\n";
}

std::string_view yaml_loading()
{
    return "def _unit(factor):\n"
           "    def _construct(loader, node):\n"
           "        return factor * float(loader.construct_scalar(node))\n"
           "    return _construct\n"
           "\n"
           "def parse_yaml(text):\n"
           "    import yaml\n"
           "    for tag, factor in _UNITS:\n"
           "        yaml.SafeLoader.add_constructor(tag, _unit(factor))\n"
           "    return yaml.load(text, Loader=yaml.SafeLoader)\n"
           "\n"
           "def _key_hint(keys):\n"
           "    names = sorted(str(k) for k in keys)\n"
           "    if not names:\n"
           "        return 'it is empty'\n"
           "    shown = 'keys are: ' + ', '.join(names[:6])\n"
           "    if len(names) > 6:\n"
           "        return shown + ' (+' + str(len(names) - 6) + ' more)'\n"
           "    return shown\n"
           "\n";
}

std::string_view yaml_list_class()
{
    return "# Ported near-verbatim from ros/xacro at revision\n"
           "# da4b3849f8320903d625250089f67f0632be86f2, xacro/__init__.py:81-110.\n"
           "class _YamlList(list):\n"
           "    @staticmethod\n"
           "    def wrap(item):\n"
           "        if isinstance(item, dict):\n"
           "            return _YamlDict(item)\n"
           "        if isinstance(item, list):\n"
           "            return _YamlList(item)\n"
           "        return item\n"
           "\n"
           "    def __getitem__(self, idx):\n"
           "        return _YamlList.wrap(list.__getitem__(self, idx))\n"
           "\n"
           "    def __iter__(self):\n"
           "        for item in list.__iter__(self):\n"
           "            yield _YamlList.wrap(item)\n"
           "\n";
}

std::string_view yaml_dict_class()
{
    return "class _YamlDict(dict):\n"
           "    def __getattr__(self, item):\n"
           "        try:\n"
           "            return _YamlList.wrap(dict.__getitem__(self, item))\n"
           "        except KeyError:\n"
           "            raise AttributeError(\"the yaml mapping has no key '\" + str(item)\n"
           "                                 + \"'; \" + _key_hint(self.keys())) from None\n"
           "\n"
           "    # The alias makes subscript and dotted access one operation, so provenance and the\n"
           "    # missing-key diagnostic reach both spellings.\n"
           "    __getitem__ = __getattr__\n";
}

}

std::string scope_prelude_source()
{
    std::string text;
    for(const std::string_view part : {scope_reads(), scope_namespace_class(), yaml_loading(), yaml_list_class(), yaml_dict_class()})
        text += part;
    return text;
}

}
