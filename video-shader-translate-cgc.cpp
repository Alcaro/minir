#include "io.h"
#include "os.h"

#define HAVE_PARAMETER_UNIFORM 1

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

#define CG_SYM_N(str, ret, name, args) ret (*name) args;
struct cglib { CG_SYMS() ndylib* lib; };
#undef CG_SYM_N
#define CG_SYM_N(str, ret, name, args) str,
static const char * const cg_names[]={ CG_SYMS() };
#undef CG_SYM_N

char * video::shader::translate_cgc(lang_t from, lang_t to, const char * text, function<char*(const char * filename)> get_include)
{
	if (from!=la_cg) return NULL;
	if (to!=la_glsl) return NULL;
	
	cglib cg;
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
#define e printf("e=%s\n",cg.GetLastListing(context));
	//TODO: process with random profile with flags "-E", "-DPARAMETER_UNIFORM", NULL to get #includes out of the way
	//use cgSetCompilerIncludeCallback and point it to get_include
	//on failure, have it hand out some random invalid string so it doesn't look in the file system, "\xEF\xBF\xBD" or "#error" maybe
	//or maybe set a flag in the caller using bind_ptr, then return NULL
	//#include "/dev/zero" must return in finite time and with finite memory use
	//#include "/etc/passwd" must fail
	
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
	
	//TODO: remove args, only -D -E -I are used and they're handled above
	static const char * args[]={ "-DPARAMETER_UNIFORM", NULL };
	CGprogram vertex = cg.CreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLV, "main_vertex", args);
	CGprogram fragment = cg.CreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLF, "main_fragment", args);
	if (!cg.IsProgramCompiled(vertex)) goto error;
	if (!cg.IsProgramCompiled(fragment)) goto error;
	
	//TODO: do this properly
	printf("%s\n", cg.GetProgramString(vertex, CG_COMPILED_PROGRAM));
	printf("%s\n", cg.GetProgramString(fragment, CG_COMPILED_PROGRAM));
	goto error;
	
	cg.DestroyContext(context);
	dylib_free(cg.lib);
return NULL;
error:
	cg.DestroyContext(context);
	}
error_noctx:
	dylib_free(cg.lib);
	
	return NULL;
}
#endif
