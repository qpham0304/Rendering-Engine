#ifndef EBO_CLASS_H
#define EBO_CLASS_H

#include<glad/glad.h>
#include "core/features/Object.h"

class EBO : public Object {
public:
    GLuint ID;
    EBO();
    EBO& operator=(const EBO& other);
    EBO(GLuint* indices, GLsizeiptr size);

    void Bind();
    void Unbind();
    void Delete();
};

#endif