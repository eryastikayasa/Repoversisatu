idf_component_register(
    SRCS
        "main.cpp"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp-sr
        uart_control
)

# ESP-SR 2.4.1 owns the model packaging flow.
# With CONFIG_PARTITION_TABLE_CUSTOM=y and a partition named "model",
# its CMakeLists.txt generates build/srmodels/srmodels.bin from
# CONFIG_SR_WN_WN9_HIESP and attaches it to the model partition.
# Do not manually package wn9_hiesp here; that would bypass the
# ESP-SR model packer and is incompatible with its srmodels format.
