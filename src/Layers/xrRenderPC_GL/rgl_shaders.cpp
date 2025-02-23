#include "stdafx.h"
#include "r2.h"

#include "Layers/xrRender/ShaderResourceTraits.h"
#include "xrCore/FileCRC32.h"

void CRender::addShaderOption(const char* name, const char* value)
{
    m_ShaderOptions += "#define ";
    m_ShaderOptions += name;
    m_ShaderOptions += " ";
    m_ShaderOptions += value;
    m_ShaderOptions += "\n";
}

template <typename T>
static GLuint create_shader(pcstr* buffer, size_t const buffer_size, cpcstr filename,
    T*& result, const GLenum* format)
{
    auto [type, output] = ShaderTypeTraits<T>::CreateHWShader(buffer, buffer_size, result->sh, format, filename);

    // Parse constant, texture, sampler binding
    if (output != 0)
    {
        result->sh = output;
        if (type == 'p')
            result->constants.parse(&output, ShaderTypeTraits<T>::GetShaderDest());
    }
    return output;
}

static GLuint create_shader(cpcstr pTarget, pcstr* buffer, size_t const buffer_size,
    cpcstr filename, void*& result, const GLenum* format)
{
    switch (pTarget[0])
    {
    case 'p':
        return create_shader(buffer, buffer_size, filename, (SPS*&)result, format);
    case 'v':
        return create_shader(buffer, buffer_size, filename, (SVS*&)result, format);
    case 'g':
        return create_shader(buffer, buffer_size, filename, (SGS*&)result, format);
    case 'c':
        return create_shader(buffer, buffer_size, filename, (SCS*&)result, format);
    case 'h':
        return create_shader(buffer, buffer_size, filename, (SHS*&)result, format);
    case 'd':
        return create_shader(buffer, buffer_size, filename, (SDS*&)result, format);
    default:
        NODEFAULT;
        return 0;
    }
}

class shader_name_holder
{
    size_t pos{};
    string_path name;

public:
    void append(cpcstr string)
    {
        const size_t size = xr_strlen(string);
        for (size_t i = 0; i < size; ++i)
        {
            name[pos] = string[i];
            ++pos;
        }
    }

    void append(u32 value)
    {
        name[pos] = '0' + char(value); // NOLINT
        ++pos;
    }

    void finish()
    {
        name[pos] = '\0';
    }

    pcstr c_str() const { return name; }
};

class shader_options_holder
{
    size_t pos{};
    string512 m_options[128];

public:
    void add(cpcstr string)
    {
        strconcat(m_options[pos++], string, "\n");
    }

    void add(cpcstr name, cpcstr value)
    {
        // It's important to have postfix increment!
        strconcat(m_options[pos++], "#define ", name, "\t", value, "\n");
    }

    void finish()
    {
        m_options[pos][0] = '\0';
    }

    [[nodiscard]] size_t size() const { return pos; }
    string512& operator[](size_t idx) { return m_options[idx]; }
};

class shader_sources_manager
{
    pcstr* m_sources{};
    size_t m_sources_lines{};
    xr_vector<pstr> m_source, m_includes;

public:
    ~shader_sources_manager()
    {
        // Free string resources
        xr_free(m_sources);
        for (pstr include : m_includes)
            xr_free(include);
        m_source.clear();
        m_includes.clear();
    }

    [[nodiscard]] auto get() const { return m_sources; }
    [[nodiscard]] auto length() const { return m_sources_lines; }

    void compile(IReader* file, shader_options_holder& options)
    {
        load_includes(file);
        apply_options(options);
    }

private:
    // TODO: OGL: make ignore commented includes
    void load_includes(IReader* file)
    {
        cpcstr sourceData = static_cast<cpcstr>(file->pointer());
        const size_t dataLength = file->length();

        // Copy source file data into a null-terminated buffer
        cpstr data = xr_alloc<char>(dataLength + 2);
        CopyMemory(data, sourceData, dataLength);
        data[dataLength] = '\n';
        data[dataLength + 1] = '\0';
        m_includes.push_back(data);
        m_source.push_back(data);

        string_path path;
        pstr str = data;
        while (strstr(str, "#include") != nullptr)
        {
            // Get filename of include directive
            str = strstr(str, "#include"); // Find the include directive
            char* fn = strchr(str, '"') + 1; // Get filename, skip quotation
            *str = '\0'; // Terminate previous source
            str = strchr(fn, '"'); // Get end of filename path
            *str = '\0'; // Terminate filename path

            // Create path to included shader
            strconcat(path, RImplementation.getShaderPath(), fn);
            FS.update_path(path, _game_shaders_, path);
            while (cpstr sep = strchr(path, '/'))
                *sep = '\\';

            // Open and read file, recursively load includes
            IReader* R = FS.r_open(path);
            R_ASSERT2(R, path);
            load_includes(R);
            FS.r_close(R);

            // Add next source, skip quotation
            ++str;
            m_source.push_back(str);
        }
    }

    void apply_options(shader_options_holder& options)
    {
        // Compile sources list
        m_sources_lines = m_source.size() + options.size();
        m_sources = xr_alloc<pcstr>(m_sources_lines);

        // Make define lines
        for (size_t i = 0; i < options.size(); ++i)
        {
            m_sources[i] = options[i];
        }
        CopyMemory(m_sources + options.size(), m_source.data(), m_source.size() * sizeof(pstr));
    }
};

HRESULT CRender::shader_compile(pcstr name, IReader* fs, pcstr pFunctionName,
    pcstr pTarget, u32 Flags, void*& result)
{
    shader_options_holder options;
    shader_name_holder sh_name;

    // Don't move these variables to lower scope!
    string64 c_name;
    string32 c_smapsize;
    string32 c_gloss;
    string32 c_sun_shafts;
    string32 c_ssao;
    string32 c_sun_quality;
    string32 c_isample;
    string32 c_water_reflection;

    // TODO: OGL: Implement these parameters.
    UNUSED(pFunctionName);
    UNUSED(Flags);

    // options:
    const auto appendShaderOption = [&](u32 option, cpcstr macro, cpcstr value)
    {
        if (option)
            options.add(macro, value);

        sh_name.append(option);
    };

    options.add("#version 410");
    options.add("#extension GL_ARB_separate_shader_objects : enable");

#ifdef DEBUG
    options.add("#pragma optimize (off)");
    sh_name.append(0u);
#else
    options.add("#pragma optimize (on)");
    sh_name.append(1u);
#endif

    xr_sprintf(c_name, "// %s.%s", name, pTarget);
    options.add(c_name);

    // Shadow map size
    {
        xr_itoa(m_SMAPSize, c_smapsize, 10);
        options.add("SMAP_size", c_smapsize);
        sh_name.append(c_smapsize);
    }

    // FP16 Filter
    appendShaderOption(o.fp16_filter, "FP16_FILTER", "1");

    // FP16 Blend
    appendShaderOption(o.fp16_blend, "FP16_BLEND", "1");

    // HW smap
    appendShaderOption(o.HW_smap, "USE_HWSMAP", "1");

    // HW smap PCF
    appendShaderOption(o.HW_smap_PCF, "USE_HWSMAP_PCF", "1");

    // Fetch4
    appendShaderOption(o.HW_smap_FETCH4, "USE_FETCH4", "1");

    // SJitter
    appendShaderOption(o.sjitter, "USE_SJITTER", "1");

    // Branching
    appendShaderOption(HW.Caps.raster_major >= 3, "USE_BRANCHING", "1");

    // Vertex texture fetch
    appendShaderOption(HW.Caps.geometry.bVTF, "USE_VTF", "1");

    // Tshadows
    appendShaderOption(o.Tshadows, "USE_TSHADOWS", "1");

    // Motion blur
    appendShaderOption(o.mblur, "USE_MBLUR", "1");

    // Sun filter
    appendShaderOption(o.sunfilter, "USE_SUNFILTER", "1");

    // Static sun on R2 and higher
    appendShaderOption(o.sunstatic, "USE_R2_STATIC_SUN", "1");

    // Force gloss
    {
        xr_sprintf(c_gloss, "%f", o.forcegloss_v);
        appendShaderOption(o.forcegloss, "FORCE_GLOSS", c_gloss);
    }

    // Force skinw
    appendShaderOption(o.forceskinw, "SKIN_COLOR", "1");

    // SSAO Blur
    appendShaderOption(o.ssao_blur_on, "USE_SSAO_BLUR", "1");

    // SSAO HDAO
    if (o.ssao_hdao)
    {
        options.add("HDAO", "1");
        sh_name.append(static_cast<u32>(1)); // HDAO on
        sh_name.append(static_cast<u32>(0)); // HBAO off
        sh_name.append(static_cast<u32>(0)); // HBAO vectorized off
    }
    else // SSAO HBAO
    {
        sh_name.append(static_cast<u32>(0)); // HDAO off
        sh_name.append(o.ssao_hbao);         // HBAO on/off

        appendShaderOption(o.ssao_hbao, "USE_HBAO", "1");
        appendShaderOption(o.hbao_vectorized, "VECTORIZED_CODE", "1");
    }

    if (o.ssao_opt_data)
    {
        if (o.ssao_half_data)
            options.add("SSAO_OPT_DATA", "2");
        else
            options.add("SSAO_OPT_DATA", "1");
    }
    sh_name.append(o.ssao_opt_data ? (o.ssao_half_data ? u32(2) : u32(1)) : u32(0));

    // skinning
    // SKIN_NONE
    appendShaderOption(m_skinning < 0, "SKIN_NONE", "1");

    // SKIN_0
    appendShaderOption(0 == m_skinning, "SKIN_0", "1");

    // SKIN_1
    appendShaderOption(1 == m_skinning, "SKIN_1", "1");

    // SKIN_2
    appendShaderOption(2 == m_skinning, "SKIN_2", "1");

    // SKIN_3
    appendShaderOption(3 == m_skinning, "SKIN_3", "1");

    // SKIN_4
    appendShaderOption(4 == m_skinning, "SKIN_4", "1");

    //	Igor: need restart options
    // Soft water
    {
        const bool softWater = RImplementation.o.advancedpp && ps_r2_ls_flags.test(R2FLAG_SOFT_WATER);
        appendShaderOption(softWater, "USE_SOFT_WATER", "1");
    }

    // Water reflections
    if (RImplementation.o.advancedpp && ps_r_water_reflection)
    {
        xr_sprintf(c_water_reflection, "%d", ps_r_water_reflection);
        options.add("SSR_QUALITY", c_water_reflection);
        sh_name.append(ps_r_water_reflection);
        const bool sshHalfDepth = ps_r2_ls_flags_ext.test(R3FLAGEXT_SSR_HALF_DEPTH);
        appendShaderOption(sshHalfDepth, "SSR_HALF_DEPTH", "1");
        const bool ssrJitter = ps_r2_ls_flags_ext.test(R3FLAGEXT_SSR_JITTER);
        appendShaderOption(ssrJitter, "SSR_JITTER", "1");
    }
    else
    {
        sh_name.append(static_cast<u32>(0));
    }

    // Soft particles
    {
        const bool useSoftParticles = RImplementation.o.advancedpp && ps_r2_ls_flags.test(R2FLAG_SOFT_PARTICLES);
        appendShaderOption(useSoftParticles, "USE_SOFT_PARTICLES", "1");
    }

    // Depth of field
    {
        const bool dof = RImplementation.o.advancedpp && ps_r2_ls_flags.test(R2FLAG_DOF);
        appendShaderOption(dof, "USE_DOF", "1");
    }

    // Sun shafts
    if (RImplementation.o.advancedpp && ps_r_sun_shafts)
    {
        xr_sprintf(c_sun_shafts, "%d", ps_r_sun_shafts);
        options.add("SUN_SHAFTS_QUALITY", c_sun_shafts);
        sh_name.append(ps_r_sun_shafts);
    }
    else
        sh_name.append(static_cast<u32>(0));

    if (RImplementation.o.advancedpp && ps_r_ssao)
    {
        xr_sprintf(c_ssao, "%d", ps_r_ssao);
        options.add("SSAO_QUALITY", c_ssao);
        sh_name.append(ps_r_ssao);
    }
    else
        sh_name.append(static_cast<u32>(0));

    // Sun quality
    if (RImplementation.o.advancedpp && ps_r_sun_quality)
    {
        xr_sprintf(c_sun_quality, "%d", ps_r_sun_quality);
        options.add("SUN_QUALITY", c_sun_quality);
        sh_name.append(ps_r_sun_quality);
    }
    else
        sh_name.append(static_cast<u32>(0));

    // Steep parallax
    {
        const bool steepParallax = RImplementation.o.advancedpp && ps_r2_ls_flags.test(R2FLAG_STEEP_PARALLAX);
        appendShaderOption(steepParallax, "ALLOW_STEEPPARALLAX", "1");
    }

    // Geometry buffer optimization
    appendShaderOption(o.gbuffer_opt, "GBUFFER_OPTIMIZATION", "1");

    // Shader Model 4.1
#ifndef XR_PLATFORM_APPLE
    appendShaderOption(o.dx11_sm4_1, "SM_4_1", "1");
    // Despite the fact that glsl 4.1 is claimed to be supported on macOS,
    // the issue is that gatherTextureOffset requires compile-time constant offset argument.
    // So it is more handy to disable its use for mac as for now.
#endif

    // Minmax SM
    appendShaderOption(o.minmax_sm, "USE_MINMAX_SM", "1");

    // Shadow of Chernobyl compatibility
    appendShaderOption(ShadowOfChernobylMode, "USE_SHOC_RESOURCES", "1");

    // add a #define for DX10_1 MSAA support
    if (o.msaa)
    {
        appendShaderOption(o.msaa, "USE_MSAA", "1");

        {
            static char samples[2];
            samples[0] = char(o.msaa_samples) + '0';
            samples[1] = 0;
            appendShaderOption(o.msaa_samples, "MSAA_SAMPLES", samples);
        }

        xr_sprintf(c_isample, "uint(%d)", m_MSAASample);
        options.add("ISAMPLE", c_isample);
        sh_name.append(static_cast<u32>(0));

        appendShaderOption(o.msaa_opt, "MSAA_OPTIMIZATION", "1");

        switch (o.msaa_alphatest)
        {
        case MSAA_ATEST_DX10_0_ATOC:
            options.add("MSAA_ALPHATEST_DX10_0_ATOC", "1");

            sh_name.append(static_cast<u32>(1)); // DX10_0_ATOC   on
            sh_name.append(static_cast<u32>(0)); // DX10_1_ATOC   off
            sh_name.append(static_cast<u32>(0)); // DX10_1_NATIVE off
            break;
        case MSAA_ATEST_DX10_1_ATOC:
            options.add("MSAA_ALPHATEST_DX10_1_ATOC", "1");

            sh_name.append(static_cast<u32>(0)); // DX10_0_ATOC   off
            sh_name.append(static_cast<u32>(1)); // DX10_1_ATOC   on
            sh_name.append(static_cast<u32>(0)); // DX10_1_NATIVE off
            break;
        case MSAA_ATEST_DX10_1_NATIVE:
            options.add("MSAA_ALPHATEST_DX10_1", "1");

            sh_name.append(static_cast<u32>(0)); // DX10_0_ATOC   off
            sh_name.append(static_cast<u32>(0)); // DX10_1_ATOC   off
            sh_name.append(static_cast<u32>(1)); // DX10_1_NATIVE on
            break;
        default:
            sh_name.append(static_cast<u32>(0)); // DX10_0_ATOC   off
            sh_name.append(static_cast<u32>(0)); // DX10_1_ATOC   off
            sh_name.append(static_cast<u32>(0)); // DX10_1_NATIVE off
        }
    }
    else
    {
        sh_name.append(static_cast<u32>(0)); // MSAA off
        sh_name.append(static_cast<u32>(0)); // No MSAA samples
        sh_name.append(static_cast<u32>(0)); // No MSAA ISAMPLE
        sh_name.append(static_cast<u32>(0)); // No MSAA optimization
        sh_name.append(static_cast<u32>(0)); // DX10_0_ATOC   off
        sh_name.append(static_cast<u32>(0)); // DX10_1_ATOC   off
        sh_name.append(static_cast<u32>(0)); // DX10_1_NATIVE off
    }

    // finish
    options.finish();
    sh_name.finish();

    char extension[3];
    strncpy_s(extension, pTarget, 2);

    u32 fileCrc = 0;
    string_path filename, full_path{};
    strconcat(sizeof(filename), filename, "gl" DELIMITER, name, ".", extension, DELIMITER, sh_name.c_str());
    if (GLAD_GL_ARB_get_program_binary && GLAD_GL_ARB_separate_shader_objects)
    {
        string_path file;
        strconcat(sizeof(file), file, "shaders_cache_oxr" DELIMITER, filename);
        FS.update_path(full_path, "$app_data_root$", file);

        string_path shadersFolder;
        FS.update_path(shadersFolder, "$game_shaders$", RImplementation.getShaderPath());

        getFileCrc32(fs, shadersFolder, fileCrc);
        fs->seek(0);
    }

    GLuint program = 0;
    if (GLAD_GL_ARB_get_program_binary && GLAD_GL_ARB_separate_shader_objects && FS.exist(full_path))
    {
        IReader* file = FS.r_open(full_path);
        if (file->length() > 8)
        {
            xr_string renderer, glVer, shadingVer;
            file->r_string(renderer);
            file->r_string(glVer);
            file->r_string(shadingVer);

            if (0 == xr_strcmp(renderer.c_str(), HW.AdapterName) &&
                0 == xr_strcmp(glVer.c_str(), HW.OpenGLVersionString) &&
                0 == xr_strcmp(shadingVer.c_str(), HW.ShadingVersion))
            {
                const GLenum binaryFormat = file->r_u32();

                const u32 savedFileCrc = file->r_u32();
                if (savedFileCrc == fileCrc)
                {
                    const u32 savedBytecodeCrc = file->r_u32();
                    const u32 bytecodeCrc = crc32(file->pointer(), file->elapsed());
                    if (bytecodeCrc == savedBytecodeCrc)
                    {
#ifdef DEBUG
                        Log("* Loading shader:", full_path);
#endif
                        program = create_shader(pTarget, (pcstr*)file->pointer(), file->elapsed(), filename, result, &binaryFormat);
                    }
                }
            }
        }
        file->close();
    }

    // Failed to use cached shader, then:
    if (!program)
    {
#ifdef DEBUG
        Log("- Compile shader:", filename);
#endif
        // Compile sources list
        shader_sources_manager sources;
        sources.compile(fs, options);

        // Compile the shader from sources
        program = create_shader(pTarget, sources.get(), sources.length(), filename, result, nullptr);

        if (GLAD_GL_ARB_get_program_binary && GLAD_GL_ARB_separate_shader_objects && program)
        {
            GLint binaryLength{};
            GLenum binaryFormat{};
            CHK_GL(glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength));

            GLvoid* binary = binaryLength ? xr_malloc(binaryLength) : nullptr;
            if (binary)
            {
                CHK_GL(glGetProgramBinary(program, binaryLength, nullptr, &binaryFormat, binary));
                IWriter* file = FS.w_open(full_path);

                file->w_string(HW.AdapterName);
                file->w_string(HW.OpenGLVersionString);
                file->w_string(HW.ShadingVersion);

                file->w_u32(binaryFormat);
                file->w_u32(fileCrc);

                const u32 bytecodeCrc = crc32(binary, binaryLength);
                file->w_u32(bytecodeCrc); // Do not write anything below this line, take a look at reading (crc32)

                file->w(binary, binaryLength);
                FS.w_close(file);
                xr_free(binary);
            }
        }
    }

    if (program)
        return S_OK;

    return E_FAIL;
}
