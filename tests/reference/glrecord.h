/*
 * Records OpenGL calls instead of executing them, for comparing the engine
 * with ep.c. Arguments are kept as raw bits so that the comparison is exact.
 */
#ifndef GL_RECORD_H
#define GL_RECORD_H


#include <stdint.h>
#include <string>
#include <vector>


struct GLCall {
	int			op;
	int			count;
	uint64_t	args[16];

	bool		operator==(const GLCall& other) const;
	std::string	ToString() const;
};

typedef std::vector<GLCall> GLTrace;

// All recorded calls go here.
extern GLTrace gTrace;


#endif	// GL_RECORD_H
