#define IMMOVABLE_CLASS(name) \
public:                       \
  name(name&&) = delete;      \
  name& operator=(name&&) = delete;

#define DEFAULT_MOVABLE_CLASS(name) \
public:                             \
  name(name&&) = default;           \
  name& operator=(name&&) = default;

#define UNCOPIABLE_CLASS(name) \
public:                        \
  name(name&) = delete;        \
  name& operator=(name&) = delete;

#define DEFAULT_COPIABLE_CLASS(name) \
public:                              \
  name(name&) = default;             \
  name& operator=(name&) = default;

#define ONLY_DEFAULT_MOVABLE_CLASS(name) \
  DEFAULT_MOVABLE_CLASS(name)            \
  UNCOPIABLE_CLASS(name)

#define ONLY_DEFAULT_COPIABLE_CLASS(name) \
  IMMOVABLE_CLASS(name)                   \
  DEFAULT_COPIABLE_CLASS(name)

#define DEFAULT_BEHAVIOR_CLASS(name) \
  DEFAULT_COPIABLE_CLASS(name)       \
  DEFAULT_MOVABLE_CLASS(name)

#define FIXED_CLASS(name) \
  IMMOVABLE_CLASS(name)   \
  UNCOPIABLE_CLASS(name)
