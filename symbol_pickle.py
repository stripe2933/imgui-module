import argparse
import os
from collections import defaultdict
from itertools import chain
import json
import re
import pickle

parser = argparse.ArgumentParser()
parser.add_argument("--branch", type=str, required=True)
args = parser.parse_args()

os.makedirs(args.branch, exist_ok=True)

with open("cimgui/imgui/imgui.h", "r") as file:
    imgui_h = file.read()
with open("cimgui/imgui/imgui_internal.h", "r") as file:
    imgui_internal_h = file.read()
with open("cimgui/imgui/misc/freetype/imgui_freetype.h", "r") as file:
    imgui_freetype_h = file.read()

def get_symbol_location(symbol: str) -> str:
    if symbol in imgui_h:
        return "imgui"
    elif symbol in imgui_internal_h:
        return "imgui_internal"
    elif symbol in imgui_freetype_h:
        return "imgui_freetype"
    else:
        raise ValueError(f"Symbol {symbol} not found in any header file.")

# ----- Process enums and structs -----

with open("cimgui/generator/output/structs_and_enums.json", "r") as file:
    data = json.load(file)

    symbol_locations: dict[str, str] = {symbol: location.split(":")[0] for symbol, location in data["locations"].items()}

    # ---- Process enums ----

    enums: defaultdict[str, list[tuple[str, list[str]]]] = defaultdict(list)
    for prefix, flag_infos in data["enums"].items():
        enums[symbol_locations[prefix]].append((prefix, [info["name"] for info in flag_infos]))

    with open(f"{args.branch}/enums.pkl", "wb") as out:
        pickle.dump(enums, out)

    # ----- Process structs -----

    # Hardcoded structs that are nested inside other structs, have to be handled separately.
    predefined_nested_structs = {
        "ImGuiTextRange"
    }

    structs: defaultdict[str, set[str]] = defaultdict(set)

    for struct_name in data["structs"]:
        if struct_name in predefined_nested_structs:
            continue # Will be implicitly exported by the parent struct.

        structs[symbol_locations[struct_name]].add(struct_name)

    for struct_name in data["templated_structs"]:
        if struct_name in predefined_nested_structs:
            continue # Will be implicitly exported by the parent struct.

        structs[get_symbol_location(struct_name)].add(struct_name)

    with open(f"{args.branch}/structs.pkl", "wb") as out:
        pickle.dump(structs, out)

# ----- Process alias types -----

with open("cimgui/generator/output/typedefs_dict.json", "r") as file:
    data = json.load(file)

    aliases: defaultdict[str, set[str]] = defaultdict(set)
    for alias, original in data.items():
        if f"struct {alias}" == original:
            continue # Already handled by the structs processing above.

        aliases[get_symbol_location(alias)].add(alias)

    with open(f"{args.branch}/aliases.pkl", "wb") as out:
        pickle.dump(aliases, out)

# ----- Process functions -----

with open("cimgui/generator/output/definitions.json", "r") as file:
    data = json.load(file)

    # Hardcoded functions that are called as ImGui::XXX(), but starts with "Im".
    # Currently cimgui does not export whether the function is in the ImGui 
    # namespace or not, therefore for now it is determined by the heuristic, by
    # checking if the function name starts with "Im". These are exceptions to 
    # the heuristic, which are in the ImGui namespace, but start with "Im".
    predefined_namespace_funcs = {
        "ImageWithBg",
        "Image",
        "ImageButton",
        "ImageButtonEx",
    }

    funcs_in_namespace: defaultdict[str, set[str]] = defaultdict(set) # Functions that are called as ImGui::XXX().
    funcs: defaultdict[str, set[str]] = defaultdict(set)
    for definition in chain.from_iterable(data.values()):
        if definition["stname"]:
            # Definition is a method of a struct, which will be implicitly exported by the struct.
            pass
        else:
            # Definition is a free function.
            location = definition["location"].split(":")[0]
            function_name = definition["funcname"]
            if function_name in predefined_namespace_funcs or not function_name.startswith("Im"):
                funcs_in_namespace[location].add(definition["funcname"])
            else:
                funcs[location].add(function_name)

    with open(f"{args.branch}/funcs.pkl", "wb") as out:
        pickle.dump(funcs, out)

    with open(f"{args.branch}/funcs_in_namespace.pkl", "wb") as out:
        pickle.dump(funcs_in_namespace, out)

# ----- Process backends -----

with open("cimgui/generator/output/impl_definitions.json", "r") as file:
    data = json.load(file)

    # cimgui does not export the type names of the impl header, therefore we need
    # to extract them from function type arguments.
    types: defaultdict[str, set[str]] = defaultdict(set)
    type_identifier_re = re.compile(r"(?:const\s+)?(?:struct\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\*?")

    # Built-in primitive types must be excluded from export.
    primitive_types = {
        "bool", "int", "float", "double", "void", "char", "unsigned char",
        "short", "unsigned short", "long", "unsigned long", "long long",
        "unsigned long long", "size_t", "uint8_t", "uint16_t", "uint32_t",
        "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "unsigned"
    }
    
    funcs: defaultdict[str, set[str]] = defaultdict(set)
    for definition in chain.from_iterable(data.values()):
        location = definition["location"].split(":")[0]
        
        for argument_info in definition["argsT"]:
            typename = type_identifier_re.match(argument_info["type"]).group(1)
            if typename not in primitive_types:
                types[location].add(typename)

        if definition["stname"]:
            # Definition is a method of a struct, which will be implicitly exported by the struct.
            pass
        else:
            funcs[location].add(definition["funcname"])

    with open(f"{args.branch}/impl_types.pkl", "wb") as out:
        pickle.dump(types, out)

    with open(f"{args.branch}/impl_funcs.pkl", "wb") as out:
        pickle.dump(funcs, out)