#ifndef FHTTPD_LOOP_H
#define FHTTPD_LOOP_H

enum loop_operation
{
	LOOP_OPERATION_NONE,
	LOOP_OPERATION_CONTINUE,
	LOOP_OPERATION_BREAK
};

typedef enum loop_operation loop_op_t;

#define LOOP_OPERATION(op)                                                                                             \
	if ((op) == LOOP_OPERATION_BREAK)                                                                                  \
		break;                                                                                                         \
	else if ((op) == LOOP_OPERATION_CONTINUE)                                                                          \
		continue;

#endif /* FHTTPD_LOOP_H */
