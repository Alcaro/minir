#include "io.h"
#include "os.h"
#include "containers.h"
#include "string.h"

#ifdef HAVE_CG_SHADERS

//TODO: This file will be translated to C once it's finished. Don't do anything too scary.

//http://http.developer.nvidia.com/Cg/cgSetCompilerIncludeString.html

#ifndef HAVE_PARAMETER_UNIFORM
#define HAVE_PARAMETER_UNIFORM 1
#endif

#define CG_EXPLICIT // disable prototypes so I don't use them by accident
#include <Cg/cg.h>

namespace {

#define CG_SYM(ret, name, args) CG_SYM_N("cg"#name, ret, name, args)
#define CG_SYMS() \
	CG_SYM(CGcontext, CreateContext, (void)) \
	CG_SYM(CGprogram, CreateProgram, (CGcontext context, CGenum program_type, const char * program, \
	                                  CGprofile profile, const char *entry, const char **args)) \
	CG_SYM(void, CompileProgram, (CGprogram program)) \
	CG_SYM(CGbool, IsProgramCompiled, (CGprogram program)) \
	CG_SYM(const char *, GetProgramString, (CGprogram program, CGenum pname)) \
	CG_SYM(void, DestroyContext, (CGcontext context)) \
	CG_SYM(void, SetCompilerIncludeCallback, (CGcontext context, CGIncludeCallbackFunc func)) \
	CG_SYM(void, SetCompilerIncludeString, (CGcontext context, const char * name, const char * source)) \
	\
	CG_SYM(const char *, GetLastListing, (CGcontext context)) \
	CG_SYM(void, SetAutoCompile, (CGcontext context, CGenum autoCompileMode)) \

#define CG_SYM_N(str, ret, name, args) ret (*name) args;
struct cglib { CG_SYMS() ndylib* lib; };
#undef CG_SYM_N
#define CG_SYM_N(str, ret, name, args) str,
static const char * const cg_names[]={ CG_SYMS() };
#undef CG_SYM_N

class cg_translator;
static ptrmap<CGcontext, cg_translator*> cgc_translators;
class cg_translator {
public:
	struct cglib cg;
	CGcontext context;
	
	function<char*(const char * filename)> get_include;
	//TODO: parameter list
	
	cg_translator()
	{
		memset(&cg, 0, sizeof(cg));
		context=NULL;
	}
	
	string preprocess(cstring text)
	{
		stringlist lines=text.split('\n');
		for (size_t i=0;i<lines.len();i++)
		{
			if (lines[i].contains("uniform") && (lines[i].contains("float4x4") || lines[i].contains("half4x4")))
			{
				lines[i]=((string)"#pragma pack_matrix(column_major)\n" + lines[i] + "\n#pragma pack_matrix(row_major)");
			}
		}
		return lines.join('\n');
	}
	
	bool load_functions()
	{
		cg.lib=dylib_create(DYLIB_MAKE_NAME("Cg"));
		if (!cg.lib) return false;
		
		funcptr* functions=(funcptr*)&cg;
		for (unsigned int i=0;i<sizeof(cg_names)/sizeof(*cg_names);i++)
		{
			functions[i]=dylib_sym_func(cg.lib, cg_names[i]);
			if (!functions[i]) return false;
		}
		return true;
	}
	
	void cgc_include_cb(const char * filename)
	{
		while (*filename=='/') filename++; // what the hell
		char* text_tmp=this->get_include(filename);
		string text=text_tmp;
		free(text_tmp);
		if (text)
		{
			cg.SetCompilerIncludeString(context, filename, preprocess(text));
		}
		else
		{
			cg.SetCompilerIncludeString(context, filename, "#error \"File not available.\"");
		}
	}
	
	static void cgc_include_cb_s(CGcontext context, const char * filename)
	{
		cgc_translators.get(context)->cgc_include_cb(filename);
	}
	
	char* translate(video::shader::lang_t from, video::shader::lang_t to, const char * text,
	                function<char*(const char * filename)> get_include)
	{
		if (from!=video::shader::la_cg) return NULL;
		if (to!=video::shader::la_glsl) return NULL;
		
		if (!load_functions()) return NULL;
		
		this->context = cg.CreateContext();
		this->get_include = get_include;
		cgc_translators.set(this->context, this);
		
#define e printf("e=%s\n",cg.GetLastListing(this->context));
		cg.SetCompilerIncludeCallback(this->context, cgc_include_cb_s);
		
		string text_p=preprocess(text);
		static const char * args[]={ "-DPARAMETER_UNIFORM", NULL };
		CGprogram vertex_p = cg.CreateProgram(this->context, CG_SOURCE, text_p, CG_PROFILE_GLSLV, "main_vertex", args);
		CGprogram fragment_p = cg.CreateProgram(this->context, CG_SOURCE, text_p, CG_PROFILE_GLSLF, "main_fragment", args);
		
		if (!cg.IsProgramCompiled(vertex_p) || !cg.IsProgramCompiled(fragment_p)) return NULL;
		
		string vertex=cg.GetProgramString(vertex_p, CG_COMPILED_PROGRAM);
		string fragment=cg.GetProgramString(fragment_p, CG_COMPILED_PROGRAM);
		
//TODO: do this properly, start at line cg2glsl.py line 581
puts(vertex);
puts(fragment);
		
puts("ALL OK");
		return NULL;
	}
	
	~cg_translator()
	{
		if (context) cgc_translators.remove(context);
		if (cg.DestroyContext && context) cg.DestroyContext(context);
		if (cg.lib) dylib_free(cg.lib);
	}
};

}

char * video::shader::translate_cgc(lang_t from, lang_t to, const char * text, function<char*(const char * filename)> get_include)
{
	cg_translator tr;
	return tr.translate(from, to, text, get_include);
}
#endif
