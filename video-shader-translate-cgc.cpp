#include "io.h"
#include "os.h"
#include "containers.h"

#ifndef HAVE_PARAMETER_UNIFORM
#define HAVE_PARAMETER_UNIFORM 1
#endif

//For easier compatibilty with RetroArch, this file may not use
//- the overloaded string operators (member functions are fine)

#ifdef HAVE_CG_SHADERS
#define CG_EXPLICIT // disable prototypes so I don't use them by accident
#include <Cg/cg.h>

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
struct cglib { CG_SYMS() ndylib* lib; } cg;
#undef CG_SYM_N
#define CG_SYM_N(str, ret, name, args) str,
static const char * const cg_names[]={ CG_SYMS() };
#undef CG_SYM_N

struct cg_include_data {
	function<char*(const char * filename)> include;
};
static ptrmap<CGcontext, struct cg_include_data*> cgc_data;
static void cgc_get_file(CGcontext context, const char * filename)
{
	struct cg_include_data * data = cgc_data.get(context);
	char * text=data->include(filename);
	if (text)
	{
		//TODO:
		//def preprocess_vertex(source_data):
		//   input_data = source_data.split('\n')
		//   ret = []
		//   for line in input_data:
		//      if ('uniform' in line) and (('float4x4' in line) or ('half4x4' in line)):
		//         ret.append('#pragma pack_matrix(column_major)\n')
		//         ret.append(line)
		//         ret.append('#pragma pack_matrix(row_major)\n')
		//      else:
		//         ret.append(line)
		//   return '\n'.join(ret)
		cg.SetCompilerIncludeString(context, filename, text);
		free(text);
	}
	else
	{
		cg.SetCompilerIncludeString(context, filename, "#error \"File not available.\"");
	}
printf("%s -> %p\n",filename,text);
}

void cgSetCompilerIncludeCallback( CGcontext context, CGIncludeCallbackFunc func );

char * video::shader::translate_cgc(lang_t from, lang_t to, const char * text, function<char*(const char * filename)> get_include)
{
	if (from!=la_cg) return NULL;
	if (to!=la_glsl) return NULL;
	
	char * ret=NULL;
	
	cg.lib=dylib_create(DYLIB_MAKE_NAME("Cg"));
	if (!cg.lib) return NULL;
	
	funcptr* functions=(funcptr*)&cg;
	for (unsigned int i=0;i<sizeof(cg_names)/sizeof(*cg_names);i++)
	{
		functions[i]=dylib_sym_func(cg.lib, cg_names[i]);
		if (!functions[i]) goto error_noctx;
	}
	
	{
	CGcontext context;
	context = cg.CreateContext();
	
	struct cg_include_data includedata={get_include};
	cgc_data.set(context, &includedata);
#define e printf("e=%s\n",cg.GetLastListing(context));
	cg.SetCompilerIncludeCallback(context, cgc_get_file);
	
	static const char * args[]={ "-DPARAMETER_UNIFORM", NULL };
e	CGprogram vertex = cg.CreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLV, "main_vertex", args);
e	CGprogram fragment = cg.CreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLF, "main_fragment", args);
e	if (!cg.IsProgramCompiled(vertex)) goto error;
e	if (!cg.IsProgramCompiled(fragment)) goto error;
	
	//TODO: do this properly
	//printf("%s\n", cg.GetProgramString(vertex, CG_COMPILED_PROGRAM));
	//printf("%s\n", cg.GetProgramString(fragment, CG_COMPILED_PROGRAM));
	
puts("ALL OK");
	goto error;
	
	if (false)
	{
	error:
		ret=NULL;
	}
	cgc_data.remove(context);
	cg.DestroyContext(context);
	}
error_noctx:
	dylib_free(cg.lib);
	
	return ret;
}
#endif
