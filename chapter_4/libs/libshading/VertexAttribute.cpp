#include "includes/opengl-common.hpp"
#include "VertexAttribute.h"

CVertexAttribute::CVertexAttribute(int location)
    : m_location(location)
{
}

bool CVertexAttribute::IsValid() const
{
    return (m_location != -1);
}

void CVertexAttribute::EnablePointer()
{
    if (!IsValid())
    {
        return;
    }
    glEnableVertexAttribArray(GLuint(m_location));
}

void CVertexAttribute::DisablePointer()
{
    if (!IsValid())
    {
        return;
    }
    glDisableVertexAttribArray(GLuint(m_location));
}

void CVertexAttribute::SetDivisor(unsigned divisor)
{
    if (!IsValid())
    {
        return;
    }
    glVertexAttribDivisor(GLuint(m_location), divisor);
}

void CVertexAttribute::SetVec3Offset(size_t offset, size_t stride, bool needClamp)
{
    if (!IsValid())
    {
        return;
    }
    const GLboolean normalize = needClamp ? GL_TRUE : GL_FALSE;
    glVertexAttribPointer(GLuint(m_location), 3, GL_FLOAT, normalize,
                          GLsizei(stride), reinterpret_cast<const void *>(offset));
}

void CVertexAttribute::SetVec2Offset(size_t offset, size_t stride)
{
    if (!IsValid())
    {
        return;
    }
    const GLboolean normalize = GL_FALSE;
    glVertexAttribPointer(GLuint(m_location), 2, GL_FLOAT, normalize,
                          GLsizei(stride), reinterpret_cast<const void *>(offset));
}

void CVertexAttribute::SetFloatsOffset(size_t offset, size_t stride, unsigned numComponents, bool needClamp)
{
    if (!IsValid())
    {
        return;
    }
    const GLboolean normalize = needClamp ? GL_TRUE : GL_FALSE;
    glVertexAttribPointer(GLuint(m_location), GLint(numComponents), GL_FLOAT, normalize,
                          GLsizei(stride), reinterpret_cast<const void *>(offset));
}

void CVertexAttribute::SetUint8Offset(size_t offset, size_t stride, unsigned numComponents)
{
    if (!IsValid())
    {
        return;
    }
    glVertexAttribIPointer(GLuint(m_location), GLint(numComponents), GL_UNSIGNED_BYTE,
                           GLsizei(stride), reinterpret_cast<const void *>(offset));
}
