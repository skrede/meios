# The grammar record's document probes: constructs of xacro's own grammar measured against the
# pinned reference, each recorded as a divergence until the two sides agree.

GRAMMAR_DOCUMENT_PROBES = ("property_evaluated_at_first_read",
                           "property_circular_definition",
                           "property_self_reference",
                           "property_never_read_undefined_name",
                           "property_never_read_dollar_run",
                           "property_default_reads_later_property",
                           "property_removed_then_read",
                           "property_value_and_remove_together",
                           "include_namespace_bare_name",
                           "include_namespace_dotted_property",
                           "include_namespace_dotted_macro",
                           "include_optional_missing_file",
                           "macro_called_by_name_attribute",
                           "element_named_by_attribute",
                           "attribute_set_on_parent",
                           "property_block_body_inserted",
                           "property_name_from_expression",
                           "property_name_not_identifier",
                           "property_name_double_underscore",
                           "root_target_namespace")
