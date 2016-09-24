#include "libchapter2_private.h"
#include "Texture2D.h"
#include "tinyxml2.h"
#include "FilesystemUtils.h"
#include <codecvt>
#include <cstdlib>
#include <boost/utility/string_ref.hpp>
#include <map>

// Did you install SDL_image development files?
// see http://www.libsdl.org/projects/SDL_image/
#include <SDL2/SDL_image.h>


using namespace tinyxml2;
using boost::filesystem::path;


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

// Convert path to UTF-8 string, acceptable by SDL_image.
std::string ConvertPathToUtf8(const path &path)
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

GLenum ConvertEnum(TextureWrapMode mode)
{
    static const std::map<TextureWrapMode, GLenum> MAPPING = {
        { TextureWrapMode::CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE },
        { TextureWrapMode::CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER },
        { TextureWrapMode::REPEAT, GL_REPEAT },
        { TextureWrapMode::MIRRORED_REPEAT, GL_MIRRORED_REPEAT },
    };
    return MAPPING.at(mode);
}

class CPropertyListParser
{
public:
    using MetaHandler = std::function<void(const std::string &textureName, glm::ivec2 &size)>;
    using FrameHandler = std::function<void(const std::string &, const CFloatRect &)>;

    CPropertyListParser(const path &xmlPath)
        : m_xmlPath(xmlPath.generic_string())
    {
    }

    void DoOnParsedTextureMeta(const MetaHandler &handler)
    {
        m_onParsedTextureMeta = handler;
    }

    void DoOnParsedFrame(const FrameHandler &handler)
    {
        m_onParsedFrame = handler;
    }

    void Parse()
    {
        const std::string xml = CFilesystemUtils::LoadFileAsString(m_xmlPath);
        XMLDocument document;
        m_error = document.Parse(xml.c_str(), xml.length());
        CheckError();

        const auto *plist = FindChild(&document, "plist");
        const auto *dict = FindChild(plist, "dict");
        ParseMetadata(GetValueNode(dict, "metadata"));
        ParseFrames(GetValueNode(dict, "frames"));
    }

private:
    const XMLElement *FindChild(const XMLNode *parent, const std::string &name)
    {
        const auto *child = parent->FirstChildElement(name.c_str());
        if (!child)
        {
            throw std::logic_error("Child element '" + name + "' not found in '"
                                   + m_xmlPath + "'");
        }
        return child;
    }

    const XMLElement *GetNextSibling(const XMLElement *node)
    {
        const auto *sibling = node->NextSiblingElement();
        if (!sibling)
        {
            std::string name = node->Name();
            throw std::logic_error("Sibling for '" + name + "' not found in '"
                                   + m_xmlPath + "'");
        }
        return sibling;
    }

    const XMLElement *GetValueNode(const XMLElement *dict, boost::string_ref key)
    {
        const auto *pKey = dict->FirstChildElement("key");
        while (pKey)
        {
            boost::string_ref currentKey = pKey->GetText();
            if (currentKey == key)
            {
                return GetNextSibling(pKey);
            }
            pKey = pKey->NextSiblingElement("key");
        }

        throw std::logic_error("Key '" + key.to_string() + "' not found in '"
                               + m_xmlPath + "'");
    }

    void ParseMetadata(const XMLElement *metadata)
    {
        const auto *pString = GetValueNode(metadata, "textureFileName");
        const std::string filename = pString->GetText();

        pString = GetValueNode(metadata, "size");
        const std::string sizeStr = pString->GetText();

        glm::ivec2 size;
        if (sscanf(sizeStr.c_str(), "{%d, %d}", &size.x, &size.y) != size.length())
        {
            throw std::logic_error("Invalid size value '" + sizeStr + "' in '"
                                   + m_xmlPath + "'");
        }
        m_onParsedTextureMeta(filename, size);
    }

    void ParseFrames(const XMLElement *frames)
    {
        const auto *pKey = frames->FirstChildElement("key");
        while (pKey)
        {
            const std::string spriteName = pKey->GetText();
            const auto *dict = GetNextSibling(pKey);
            CFloatRect rect = ParseFrameRect(dict);
            m_onParsedFrame(spriteName, rect);
            pKey = pKey->NextSiblingElement("key");
        }
    }

    CFloatRect ParseFrameRect(const XMLElement *dict)
    {
        const auto *nodeX = GetValueNode(dict, "x");
        const auto *nodeY = GetValueNode(dict, "y");
        const auto *nodeWidth = GetValueNode(dict, "width");
        const auto *nodeHeight = GetValueNode(dict, "height");

        const auto x = std::stoul(nodeX->GetText());
        const auto y = std::stoul(nodeY->GetText());
        const auto width = std::stoul(nodeWidth->GetText());
        const auto height = std::stoul(nodeHeight->GetText());

        return CFloatRect(glm::vec2{float(x), float(y)},
                          glm::vec2{float(x + width), float(y + height)});
    }

    void CheckError()
    {
        if (m_error != XML_SUCCESS)
        {
            const std::string errorCode = std::to_string(unsigned(m_error));
            throw std::runtime_error("Failed to load plist file '" + m_xmlPath
                                     + "': xml error #" + errorCode);
        }
    }

    MetaHandler m_onParsedTextureMeta;
    FrameHandler m_onParsedFrame;
    std::string m_xmlPath;
    XMLError m_error = XML_SUCCESS;
};
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

CTexture2DAtlas::CTexture2DAtlas(const path &xmlPath, CTexture2DLoader loader)
{
    const path abspath = CFilesystemUtils::GetResourceAbspath(xmlPath);
    glm::vec2 frameScale;

    CPropertyListParser parser(abspath);
    parser.DoOnParsedTextureMeta([&](const std::string &filename, const glm::ivec2 &size) {
        // Запоминаем коэффициенты для преобразования всех координат
        //    в атласе текстур к диапазону [0..1].
        frameScale = { float(1.f / size.x),
                       float(1.f / size.y) };
        m_pTexture = loader.Load(abspath.parent_path() / filename);
    });
    parser.DoOnParsedFrame([&](const std::string &name, const CFloatRect &rect) {
        // Преобразуем координаты в атласе текстур к диапазону [0..1]
        CFloatRect texRect = rect.GetScaled(frameScale);
        // Переворачиваем по оси Y, чтобы синхронизировать
        //    с переворотом текстуры в загузчике текстур.
        const float flippedY = 1.f - texRect.GetBottomRight().y;
        texRect.MoveTo({ texRect.GetTopLeft().x, flippedY });

        m_frames[name] = texRect;
    });
    parser.Parse();
}

const CTexture2D &CTexture2DAtlas::GetTexture() const
{
    return *m_pTexture;
}

CFloatRect CTexture2DAtlas::GetFrameRect(const std::string &frameName) const
{
    return m_frames.at(frameName);
}

CTexture2DUniquePtr CTexture2DLoader::Load(const path &path)
{
    const std::string pathUtf8 = ConvertPathToUtf8(path);
    SDLSurfacePtr pSurface(IMG_Load(pathUtf8.c_str()));
    if (!pSurface)
    {
        throw std::runtime_error("Cannot find texture at " + path.generic_string());
    }

    const bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(pSurface->format->format);
    const GLenum pixelFormat = hasAlpha ? GL_RGBA : GL_RGB;
    const uint32_t requiredFormat = hasAlpha
            ? SDL_PIXELFORMAT_ABGR8888
            : SDL_PIXELFORMAT_RGB24;

    if (pSurface->format->format != requiredFormat)
    {
        pSurface.reset(SDL_ConvertSurfaceFormat(pSurface.get(), requiredFormat, 0));
    }

    FlipSurfaceVertically(*pSurface);

    auto pTexture = std::make_unique<CTexture2D>();
    pTexture->DoWhileBinded([&] {
        glTexImage2D(GL_TEXTURE_2D, 0, GLint(pixelFormat), pSurface->w, pSurface->h,
                     0, pixelFormat, GL_UNSIGNED_BYTE, pSurface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(ConvertEnum(m_wrapS)));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(ConvertEnum(m_wrapT)));
    });

    return pTexture;
}

void CTexture2DLoader::SetWrapMode(TextureWrapMode wrap)
{
    m_wrapS = wrap;
    m_wrapT = wrap;
}

void CTexture2DLoader::SetWrapMode(TextureWrapMode wrapS, TextureWrapMode wrapT)
{
    m_wrapS = wrapS;
    m_wrapT = wrapT;
}