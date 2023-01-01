#pragma once

inline char const ERR_OUT_OF_SCOPE_DIFFABLE[] =
    "field type declaration is out of scope and is required to be implicitly diffable";
inline char const ERR_MAP_DISALLOWED[] = "map is not allowed in diffable types";
inline char const ERR_ONEOF_DISALLOWED[] = "oneof is not allowed in diffable types";
inline char const ERR_GROUP_DISALLOWED[] = "group is not allowed in diffable types";
inline char const ERR_REPEATED_SCALARS_DISALLOWED[] =
    "repeated scalars are not allowed in diffable types";
inline char const ERR_NONINTEGRAL_ID[] = "type of field 'id' should be integral";
inline char const ERR_NO_ID[] = "field 'id' is required in diffable types";
