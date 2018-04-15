
#define DSK_CONTAINED_ARRAY_DECLARE_AND_INIT(name, type, initial_size) \
  struct {                                                    \
    size_t length;                                            \
    size_t alloced;                                           \
    type *data;                                               \
  } name = {0,(initial_size),NULL}
#define DSK_CONTAINED_ARRAY_DECLARE(name, type)               \
  struct {                                                    \
    size_t length;                                            \
    size_t alloced;                                           \
    type *data;                                               \
  } name;
#define DSK_CONTAINED_ARRAY_INIT(name, initial_size)          \
  do{                                                         \
    (name).length = 0;                                        \
    (name).alloced = initial_size;                            \
    (name).data = NULL;                                       \
  }while(0)
#define DSK_CONTAINED_ARRAY_INITIALIZER(initial_size)         \
  { 0, (initial_size), NULL }

#define DSK_CONTAINED_ARRAY_APPEND(name, value)               \
do{                                                           \
  if ((name).data == NULL)                                    \
    {                                                         \
      size_t s = sizeof ((name).data[0]) * (name).alloced;    \
      (name).data = dsk_malloc (s);                           \
    }                                                         \
  else if ((name).alloced == (name).length)                   \
    {                                                         \
      (name).alloced *= 2;                                    \
      size_t s = sizeof ((name).data[0]) * (name).alloced;    \
      (name).data = dsk_realloc ((name).data, s);             \
    }                                                         \
  (name).data[(name).length++] = (value);                     \
}while(0)
