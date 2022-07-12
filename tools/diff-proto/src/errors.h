#pragma once

inline const char ERR_OUT_OF_SCOPE_DIFFABLE[] =
    "field type declaration is out of scope and is required to be implicitly diffable";
inline const char ERR_MAP_DISALLOWED[] = "map is not allowed in diffable types";
inline const char ERR_ONEOF_DISALLOWED[] = "oneof is not allowed in diffable types";
inline const char ERR_GROUP_DISALLOWED[] = "group is not allowed in diffable types";
inline const char ERR_REPEATED_SCALARS_DISALLOWED[] =
    "repeated scalars are not allowed in diffable types";
inline const char ERR_NONINTEGRAL_ID[] = "type of field 'id' should be integral";
inline const char ERR_NO_ID[] = "field 'id' is required in diffable types";
