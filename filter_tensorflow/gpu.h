
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void print_me();
EXTERNC TfLiteStatus TfLiteInterpreterModifyGraphWithDelegate(const TfLiteInterpreter* interpreter, TfLiteDelegate* delegate);

#undef EXTERNC
