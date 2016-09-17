#include "libchapter3_private.h"
#include "Texture2D.h"
#include <codecvt>
#include <SDL2/SDL_image.h>
#include <cstdlib>

namespace
{
struct SDLSurfaceDeleter
{
	void operator()(SDL_Surface *ptr)
	{
		SDL_FreeSurface(ptr);
	}
};
// Используем unique_ptr с явно заданной функцией удаления вместо delete.
using SDLSurfacePtr = std::unique_ptr<SDL_Surface, SDLSurfaceDeleter>;

// Convert boost::filesystem::path to UTF-8 string, acceptable by SDL_image.
std::string ConvertPathToUtf8(const boost::filesystem::path &path)
{
#ifdef _WIN32
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(path.native());
#else
	return path.native();
#endif
}

void FlipSurfaceVertically(SDL_Surface & surface)
{
	const auto rowSize = size_t(surface.w * surface.format->BytesPerPixel);
	std::vector<uint8_t> row(rowSize);

	for (size_t y = 0, height = size_t(surface.h); y < height / 2; ++y)
	{
		auto *pixels = reinterpret_cast<uint8_t*>(surface.pixels);
		auto *upperRow = pixels + rowSize * y;
		auto *lowerRow = pixels + rowSize * (height - y - 1);
		std::memcpy(row.data(), upperRow, rowSize);
		std::memcpy(upperRow, lowerRow, rowSize);
		std::memcpy(lowerRow, row.data(), rowSize);
	}
}
}


CTexture2D::CTexture2D()
{
    glGenTextures(1, &m_textureId);
}

CTexture2D::~CTexture2D()
{
    glDeleteTextures(1, &m_textureId);
}

void CTexture2D::Bind() const
{
    glBindTexture(GL_TEXTURE_2D, m_textureId);
}

void CTexture2D::Unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

CTexture2DUniquePtr LoadTexture2D(boost::filesystem::path &path)
{
	const std::string pathUtf8 = ConvertPathToUtf8(path);
    SDLSurfacePtr pSurface(IMG_Load(pathUtf8.c_str()));
    if (!pSurface)
    {
        throw std::runtime_error("Cannot find texture at " + path.generic_string());
    }

    const GLenum pixelFormat = GL_RGB;
    switch (pSurface->format->format)
    {
    case SDL_PIXELFORMAT_RGB24:
        break;
    case SDL_PIXELFORMAT_BGR24:
        pSurface.reset(SDL_ConvertSurfaceFormat(pSurface.get(),
                                                SDL_PIXELFORMAT_RGB24, 0));
        break;
    default:
    {
        std::string name = SDL_GetPixelFormatName(pSurface->format->format);
        throw std::runtime_error("Unsupported image pixel format " + name
                                 + " at " + path.generic_string());
    }
    }

    FlipSurfaceVertically(*pSurface);

    auto pTexture = std::make_unique<CTexture2D>();
    pTexture->DoWhileBinded([&] {
        glTexImage2D(GL_TEXTURE_2D, 0, GLint(pixelFormat), pSurface->w, pSurface->h,
                     0, pixelFormat, GL_UNSIGNED_BYTE, pSurface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    });

    return pTexture;
}
