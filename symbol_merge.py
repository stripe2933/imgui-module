from collections import defaultdict
import pickle
import os

class OutputDict(defaultdict):
    def __missing__(self, key):
        self[key] = ("module;\n"
                     "\n"
                     f"#include <{key}.h>\n"
                     "\n"
                     f"export module {key};\n\n")

        if key == "imgui":
            # cimgui does not handle math operators at all, so we need to manually export them.
            self[key] += ("export {\n"
                          "#ifdef IMGUI_DEFINE_MATH_OPERATORS\n"
                          "    using ::operator+;\n"
                          "    using ::operator-;\n"
                          "    using ::operator*;\n"
                          "    using ::operator/;\n"
                          "    using ::operator+=;\n"
                          "    using ::operator-=;\n"
                          "    using ::operator*=;\n"
                          "    using ::operator/=;\n"
                          "    using ::operator==;\n"
                          "    using ::operator!=;\n"
                          "#endif\n\n")
        else:
            self[key] += ("export import imgui;\n"
                          "\n"
                          "export {\n")

        return self[key]

outputs = OutputDict()

# ----- Process enums -----

with open("main/enums.pkl", "rb") as file:
    enums_main = pickle.load(file)

with open("docking/enums.pkl", "rb") as file:
    enums_docking = pickle.load(file)

assert enums_main.keys() == enums_docking.keys(), "The set of headers in main and docking enums must be the same."

for location in enums_main.keys():
    outputs[location] += "    // ----- Enums -----\n"

    prefixes_main = {prefix for prefix, _ in enums_main[location]}
    prefixes_docking = dict(enums_docking[location])

    for prefix, flags in enums_main[location]:
        if prefix in prefixes_main and prefix in prefixes_docking:
            outputs[location] += f"\n    using ::{prefix};\n"

            flags_main = set(flags)
            flags_docking = set(prefixes_docking[prefix])

            # flags in both main and docking
            for flag in flags:
                if flag in flags_docking:
                    outputs[location] += f"    using ::{flag};\n"

            # flags only in docking
            has_docking_only_flag = False
            for flag in prefixes_docking[prefix]:
                if flag not in flags_main:
                    if not has_docking_only_flag:
                        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
                        has_docking_only_flag = True

                    outputs[location] += f"    using ::{flag};\n"

            # flags only in main
            has_main_only_flag = False
            for flag in flags:
                if flag not in flags_docking:
                    if not has_main_only_flag:
                        if has_docking_only_flag:
                            outputs[location] += "#else\n"
                        else:
                            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"
                        has_main_only_flag = True

                    outputs[location] += f"    using ::{flag};\n"

            if has_docking_only_flag or has_main_only_flag:
                outputs[location] += "#endif\n"

    prefixes_only_in_docking = prefixes_docking.keys() - prefixes_main
    if prefixes_only_in_docking:
        outputs[location] += "\n#ifdef IMGUI_HAS_DOCK"

        for prefix, flags in enums_docking[location]:
            if prefix not in prefixes_only_in_docking:
                continue

            outputs[location] += f"\n    using ::{prefix};\n"
            for flag in flags:
                outputs[location] += f"    using ::{flag};\n"

    prefixes_only_in_main = prefixes_main - prefixes_docking.keys()
    if prefixes_only_in_main:
        if prefixes_only_in_docking:
            outputs[location] += "#else"
        else:
            outputs[location] += "\n#ifndef IMGUI_HAS_DOCK"

        for prefix, flags in enums_main[location]:
            if prefix not in prefixes_only_in_main:
                continue

            outputs[location] += f"\n    using ::{prefix};\n"
            for flag in flags:
                outputs[location] += f"    using ::{flag};\n"

    if prefixes_only_in_docking or prefixes_only_in_main:
        outputs[location] += "#endif\n"


# ----- Process structs -----

with open("main/structs.pkl", "rb") as file:
    structs_main = pickle.load(file)

with open("docking/structs.pkl", "rb") as file:
    structs_docking = pickle.load(file)

assert structs_main.keys() == structs_docking.keys(), "The set of headers in main and docking structs must be the same."

for location in structs_main.keys():
    outputs[location] += "\n    // ----- Structs -----\n\n"

    for struct in sorted(structs_main[location] & structs_docking[location]):
        outputs[location] += f"    using ::{struct};\n"

    structs_only_in_docking = sorted(structs_docking[location] - structs_main[location])
    if structs_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for struct in structs_only_in_docking:
            outputs[location] += f"    using ::{struct};\n"

    structs_only_in_main = sorted(structs_main[location] - structs_docking[location])
    if structs_only_in_main:
        if structs_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"

        for struct in structs_only_in_main:
            outputs[location] += f"    using ::{struct};\n"

    if structs_only_in_docking or structs_only_in_main:
        outputs[location] += "#endif\n"

# ----- Process alias types -----

with open("main/aliases.pkl", "rb") as file:
    aliases_main = pickle.load(file)

with open("docking/aliases.pkl", "rb") as file:
    aliases_docking = pickle.load(file)

assert aliases_main.keys() == aliases_docking.keys(), "The set of headers in main and docking alias types must be the same."

for location in aliases_main.keys():
    outputs[location] += "\n    // ----- Type aliases -----\n\n"

    for alias in sorted(aliases_main[location] & aliases_docking[location]):
        outputs[location] += f"    using ::{alias};\n"

    aliases_only_in_docking = sorted(aliases_docking[location] - aliases_main[location])
    if aliases_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for alias in aliases_only_in_docking:
            outputs[location] += f"    using ::{alias};\n"

    aliases_only_in_main = sorted(aliases_main[location] - aliases_docking[location])
    if aliases_only_in_main:
        if aliases_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"

        for alias in aliases_only_in_main:
            outputs[location] += f"    using ::{alias};\n"

    if aliases_only_in_docking or aliases_only_in_main:
        outputs[location] += "#endif\n"

# ----- Process functions -----

with open("main/funcs.pkl", "rb") as file:
    funcs_main = pickle.load(file)

with open("docking/funcs.pkl", "rb") as file:
    funcs_docking = pickle.load(file)

assert funcs_main.keys() == funcs_docking.keys(), "The set of headers in main and docking functions must be the same."

with open("main/funcs_in_namespace.pkl", "rb") as file:
    funcs_in_namespace_main = pickle.load(file)

with open("docking/funcs_in_namespace.pkl", "rb") as file:
    funcs_in_namespace_docking = pickle.load(file)

assert funcs_in_namespace_main.keys() == funcs_in_namespace_docking.keys(), "The set of headers in main and docking functions in namespace must be the same."

for location in funcs_main.keys() | funcs_in_namespace_main.keys():
    outputs[location] += "\n    // ----- Functions -----\n\n"

for location in funcs_main.keys():
    for function in sorted(funcs_main[location] & funcs_docking[location]):
        outputs[location] += f"    using ::{function};\n"

    funcs_only_in_docking = sorted(funcs_docking[location] - funcs_main[location])
    if funcs_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for function in funcs_only_in_docking:
            outputs[location] += f"    using ::{function};\n"

    funcs_only_in_main = sorted(funcs_main[location] - funcs_docking[location])
    if funcs_only_in_main:
        if funcs_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"

        for function in funcs_only_in_main:
            outputs[location] += f"    using ::{function};\n"

    if funcs_only_in_docking or funcs_only_in_main:
        outputs[location] += "#endif\n"

for location in funcs_in_namespace_main.keys():
    if outputs[location][-2:] != "\n\n":
        outputs[location] += "\n"
    outputs[location] += "namespace ImGui {\n"

    for function in sorted(funcs_in_namespace_main[location] & funcs_in_namespace_docking[location]):
        outputs[location] += f"    using ImGui::{function};\n"

    funcs_in_namespace_only_in_docking = sorted(funcs_in_namespace_docking[location] - funcs_in_namespace_main[location])
    if funcs_in_namespace_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for function in funcs_in_namespace_only_in_docking:
            outputs[location] += f"    using ImGui::{function};\n"

    funcs_in_namespace_only_in_main = sorted(funcs_in_namespace_main[location] - funcs_in_namespace_docking[location])
    if funcs_in_namespace_only_in_main:
        if funcs_in_namespace_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"

        for function in funcs_in_namespace_only_in_main:
            outputs[location] += f"    using ImGui::{function};\n"

    if funcs_in_namespace_only_in_docking or funcs_in_namespace_only_in_main:
        outputs[location] += "#endif\n"

    if location == "imgui":
        # IMGUI_CHECKVERSION() is a macro that cannot be exported by module.
        # For workaround, we define a function that calls the macro.
        outputs[location] += ("\n"
                              "    /**\n"
                              "     * @brief Use this for the replacement of <tt>IMGUI_CHECKVERSION()</tt>.\n"
                              "     */\n"
                              "    void CheckVersion() { IMGUI_CHECKVERSION(); };\n")

    outputs[location] += "}\n"

for location in outputs:
    outputs[location] += "}\n"

for location, output in outputs.items():
    os.makedirs("generated", exist_ok=True)
    with open(f"generated/{location}.cppm", "w") as file:
        file.write(output)

# ----- Process backends -----

outputs.clear()

with open("main/impl_types.pkl", "rb") as file:
    impl_types_main = pickle.load(file)

with open("docking/impl_types.pkl", "rb") as file:
    impl_types_docking = pickle.load(file)

assert impl_types_main.keys() == impl_types_docking.keys(), "The set of headers in main and docking implementation types must be the same."

for location in impl_types_main.keys():
    outputs[location] += "    // ----- Types -----\n\n"

    for impl_type in sorted(impl_types_main[location] & impl_types_docking[location]):
        outputs[location] += f"    using ::{impl_type};\n"

    impl_types_only_in_docking = sorted(impl_types_docking[location] - impl_types_main[location])
    if impl_types_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for impl_type in impl_types_only_in_docking:
            outputs[location] += f"    using ::{impl_type};\n"

    impl_types_only_in_main = sorted(impl_types_main[location] - impl_types_docking[location])
    if impl_types_only_in_main:
        if impl_types_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "\n#ifndef IMGUI_HAS_DOCK\n"

        for impl_type in impl_types_only_in_main:
            outputs[location] += f"    using ::{impl_type};\n"

    if impl_types_only_in_docking or impl_types_only_in_main:
        outputs[location] += "#endif\n"

with open("main/impl_funcs.pkl", "rb") as file:
    impl_funcs_main = pickle.load(file)

with open("docking/impl_funcs.pkl", "rb") as file:
    impl_funcs_docking = pickle.load(file)

assert impl_funcs_main.keys() == impl_funcs_docking.keys(), "The set of headers in main and docking implementation functions must be the same."

for location in impl_funcs_main.keys():
    outputs[location] += "\n    // ----- Functions -----\n\n"

    for impl_func in sorted(impl_funcs_main[location] & impl_funcs_docking[location]):
        outputs[location] += f"    using ::{impl_func};\n"

    impl_funcs_only_in_docking = sorted(impl_funcs_docking[location] - impl_funcs_main[location])
    if impl_funcs_only_in_docking:
        outputs[location] += "#ifdef IMGUI_HAS_DOCK\n"
        for impl_func in impl_funcs_only_in_docking:
            outputs[location] += f"    using ::{impl_func};\n"

    impl_funcs_only_in_main = sorted(impl_funcs_main[location] - impl_funcs_docking[location])
    if impl_funcs_only_in_main:
        if impl_funcs_only_in_docking:
            outputs[location] += "#else\n"
        else:
            outputs[location] += "#ifndef IMGUI_HAS_DOCK\n"

        for impl_func in impl_funcs_only_in_main:
            outputs[location] += f"    using ::{impl_func};\n"

    if impl_funcs_only_in_docking or impl_funcs_only_in_main:
        outputs[location] += "#endif\n"

    outputs[location] += "}\n"

for location, output in outputs.items():
    os.makedirs("generated/backends", exist_ok=True)
    with open(f"generated/backends/{location}.cppm", "w") as file:
        file.write(output)