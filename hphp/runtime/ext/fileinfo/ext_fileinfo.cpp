#include "hphp/runtime/base/base-includes.h"
#include "hphp/runtime/base/stream-wrapper-registry.h"
#include "hphp/runtime/ext/fileinfo/libmagic/magic.h"

namespace HPHP {
const StaticString s_finfo("finfo");

class FileinfoResource : public SweepableResourceData {
public:
  DECLARE_RESOURCE_ALLOCATION(FileinfoResource)
  CLASSNAME_IS("file_info")
  virtual const String& o_getClassNameHook() const { return classnameof(); }

  explicit FileinfoResource(struct magic_set* magic) : m_magic(magic) {}
  virtual ~FileinfoResource() { close(); }
  void close() {
    magic_close(m_magic);
    m_magic = nullptr;
  }
  struct magic_set* getMagic() { return m_magic; }

private:
  struct magic_set* m_magic;
};

void FileinfoResource::sweep() {
  close();
}

static Variant HHVM_FUNCTION(finfo_open,
    int64_t options,
    const String& magic_file) {
  auto magic = magic_open(options);
  if (magic == nullptr) {
    raise_warning("Invalid mode '%" PRId64 "'.", options);
    return false;
  }

  auto fn = magic_file.empty() ? nullptr : magic_file.data();
  if (magic_load(magic, fn) == -1) {
    raise_warning("Failed to load magic database at '%s'.", magic_file.data());
    magic_close(magic);
    return false;
  }

  return NEWOBJ(FileinfoResource)(magic);
}

static bool HHVM_FUNCTION(finfo_close, CResRef finfo) {
  auto res = finfo.getTyped<FileinfoResource>();
  if (!res) {
    return false;
  }
  res->close();
  return true;
}

static bool HHVM_FUNCTION(finfo_set_flags, CResRef finfo, int64_t options) {
  auto magic = finfo.getTyped<FileinfoResource>()->getMagic();
  if (magic_setflags(magic, options) == -1) {
    raise_warning(
      "Failed to set option '%" PRId64 "' %d:%s",
      options,
      magic_errno(magic),
      magic_error(magic)
    );
    return false;
  }
  return true;
}

#define FILEINFO_MODE_BUFFER 0
#define FILEINFO_MODE_STREAM 1
#define FILEINFO_MODE_FILE 2

static Variant php_finfo_get_type(
    const CResRef& object, const Variant& what,
    int64_t options, CResRef context, int mode, int mimetype_emu)
{
  String ret_val;
  String buffer;
  char mime_directory[] = "directory";
  struct magic_set *magic = NULL;

  if (mimetype_emu) {
    if (what.isString()) {
      buffer = what.toString();
      mode = FILEINFO_MODE_FILE;
    } else if (what.isResource()) {
      mode = FILEINFO_MODE_STREAM;
    } else {
      raise_warning("Can only process string or stream arguments");
    }

    magic = magic_open(MAGIC_MIME_TYPE);
    if (magic_load(magic, NULL) == -1) {
      raise_warning("Failed to load magic database.");
      goto common;
    }
  } else if (object.get()) {
    buffer = what.toString();
    magic = object.getTyped<FileinfoResource>()->getMagic();
  } else {
    // if we want to support finfo as a resource as well, do it here
    not_reached();
  }

  // Set options for the current file/buffer.
  if (options) {
    HHVM_FN(finfo_set_flags)(object, options);
  }

  switch (mode) {
    case FILEINFO_MODE_BUFFER:
    {
      ret_val = magic_buffer(magic, buffer.data(), buffer.size());
      break;
    }

    case FILEINFO_MODE_STREAM:
    {
      auto stream = what.toResource().getTyped<File>();
      if (!stream) {
        goto common;
      }

      auto streampos = stream->tell(); // remember stream position
      stream->seek(0, SEEK_SET);

      ret_val = magic_stream(magic, stream);

      stream->seek(streampos, SEEK_SET);
      break;
    }

    case FILEINFO_MODE_FILE:
    {
      if (buffer.empty()) {
        raise_warning("Empty filename or path");
        ret_val = null_string;
        goto clean;
      }

      auto wrapper = Stream::getWrapperFromURI(buffer);
      auto stream = wrapper->open(buffer, "rb", 0, HPHP::Variant());

      if (!stream) {
        ret_val = null_string;
        goto clean;
      }

      struct stat st;
      if (wrapper->stat(buffer, &st) == 0) {
        if (st.st_mode & S_IFDIR) {
          ret_val = mime_directory;
        } else {
          ret_val = magic_stream(magic, stream);
        }
      }

      stream->close();
      break;
    }

    default:
      raise_warning("Can only process string or stream arguments");
  }

common:
  if (!ret_val) {
    raise_warning(
      "Failed identify data %d:%s",
      magic_errno(magic),
      magic_error(magic)
    );
  }

clean:
  if (mimetype_emu) {
    magic_close(magic);
  }

  // Restore options
  if (options) {
    HHVM_FN(finfo_set_flags)(object, options);
  }

  if (ret_val) {
    return ret_val;
  }
  return false;
}

static String HHVM_FUNCTION(finfo_buffer,
    CResRef finfo, const String& string,
    int64_t options, CResRef context) {
  return php_finfo_get_type(
      finfo, string, options, context,
      FILEINFO_MODE_BUFFER, 0);
}

static String HHVM_FUNCTION(finfo_file,
    CResRef finfo, const String& file_name,
    int64_t options, CResRef context) {
  return php_finfo_get_type(
      finfo, file_name, options, context,
      FILEINFO_MODE_FILE, 0);
}

static String HHVM_FUNCTION(mime_content_type, const Variant& filename) {
  return php_finfo_get_type(nullptr, filename, 0, nullptr, -1, 1);
}

//////////////////////////////////////////////////////////////////////////////

const StaticString s_FILEINFO_NONE("FILEINFO_NONE");
const StaticString s_FILEINFO_SYMLINK("FILEINFO_SYMLINK");
const StaticString s_FILEINFO_MIME("FILEINFO_MIME");
const StaticString s_FILEINFO_MIME_TYPE("FILEINFO_MIME_TYPE");
const StaticString s_FILEINFO_MIME_ENCODING("FILEINFO_MIME_ENCODING");
const StaticString s_FILEINFO_DEVICES("FILEINFO_DEVICES");
const StaticString s_FILEINFO_CONTINUE("FILEINFO_CONTINUE");
const StaticString s_FILEINFO_PRESERVE_ATIME("FILEINFO_PRESERVE_ATIME");
const StaticString s_FILEINFO_RAW("FILEINFO_RAW");

class fileinfoExtension : public Extension {
 public:
  fileinfoExtension() : Extension("fileinfo") {}
  virtual void moduleInit() {
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_NONE.get(), MAGIC_NONE
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_SYMLINK.get(), MAGIC_SYMLINK
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_MIME.get(), MAGIC_MIME
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_MIME_TYPE.get(), MAGIC_MIME_TYPE
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_MIME_ENCODING.get(),MAGIC_MIME_ENCODING
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_DEVICES.get(), MAGIC_DEVICES
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_CONTINUE.get(), MAGIC_CONTINUE
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_PRESERVE_ATIME.get(), MAGIC_PRESERVE_ATIME
    );
    Native::registerConstant<KindOfInt64>(
      s_FILEINFO_RAW.get(), MAGIC_RAW
    );
    HHVM_FE(finfo_open);
    HHVM_FE(finfo_buffer);
    HHVM_FE(finfo_file);
    HHVM_FE(finfo_set_flags);
    HHVM_FE(finfo_close);
    HHVM_FE(mime_content_type);
    loadSystemlib();
  }
} s_fileinfo_extension;

// Uncomment for non-bundled module
//HHVM_GET_MODULE(fileinfo);

//////////////////////////////////////////////////////////////////////////////
} // namespace HPHP
