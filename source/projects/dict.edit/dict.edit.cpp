/**
	@file
	dictedit - demonstrate use of jdataview
	Copyright 2020 - Cycling '74
	Timothy Place, tim@cycling74.com
*/

#include "c74_ui_dataview.h"

using namespace c74::max;

enum {
	kOutletParameterNames = 0,
	kOutletParameterValues,
	kOutletPresetNames,
	kOutletPluginNames,
	kNumContolOutlets
};


typedef struct _dictedit_column {
	t_symbol*	name;
	t_atom_long	index;
	t_object*	dataview_column;
	t_symbol*	type;
	t_atom_long	range[2];
	bool		use_range;
} t_dictedit_column;

typedef struct _dictedit {
	t_jbox			d_box;
	void*			d_outlet;
	t_object*		d_dataview;		///< The dataview object
	t_hashtab*		d_columns;		///< The dataview columns:  column name -> column index
	t_dictionary*	d_dict;
	t_symbol*		d_name;
} t_dictedit;


// general prototypes
void		*dictedit_new(t_symbol *s, short argc, t_atom *argv);
void		dictedit_initdataview(t_dictedit *x);
void		dictedit_free(t_dictedit *x);
void		dictedit_newpatcherview(t_dictedit *x, t_object *patcherview);
void		dictedit_freepatcherview(t_dictedit *x, t_object *patcherview);
t_max_err	dictedit_notify(t_dictedit *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
void		dictedit_assist(t_dictedit *x, void *b, long m, long a, char *s);
void		dictedit_bang(t_dictedit *x);
void		dictedit_getcelltext(t_dictedit *x, t_symbol *colname, t_rowref rr, char *text, long maxlen);
void		dictedit_getcellvalue(t_dictedit *x, t_symbol *colname, t_rowref rr, long *argc, t_atom *argv);

void		dictedit_setvalue(t_dictedit *x, t_symbol *colname, t_rowref rr, long argc, t_atom *argv);
void		dictedit_selectedrow(t_dictedit* self, t_rowref* rr);

t_max_err	dictedit_set_dictionary(t_dictedit *x, void *attr, long argc, t_atom *argv);
void		dictedit_toggle_component(t_object *x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label);
void		dictedit_menu_component(t_object* self, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label);
void		dictedit_text_component(t_object* x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label);
void		dictedit_integer_component(t_object* x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label);
void		dictedit_getcellmenu(t_dictedit *x, t_symbol *colname, t_rowref rr, long *argc, t_atom *argv, char **enabled, long *currentitemindex);


static t_class	*s_dictedit_class = NULL;
static t_symbol	*ps_nothing;
static t_symbol	*ps_modified;
static t_symbol	*ps_name;
static t_symbol	*ps_type;


/************************************************************************************/

void ext_main(void* r) {
	t_class* c = class_new("dict.edit", (method)dictedit_new, (method)dictedit_free, sizeof(t_dictedit), (method)0L, A_GIMME, 0);
	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;

	jbox_initclass(c, JBOX_COLOR);

	class_addmethod(c, (method)dictedit_bang,				"bang",							0);			// refresh
	class_addmethod(c, (method)dictedit_getcelltext,		"getcelltext",					A_CANT, 0);
	class_addmethod(c, (method)dictedit_getcellvalue,		"getcellvalue",					A_CANT, 0);
	class_addmethod(c, (method)dictedit_setvalue,			"setvalue",						A_CANT, 0);
	class_addmethod(c, (method)dictedit_selectedrow,		"selectedrow",					A_CANT, 0);
	
	class_addmethod(c, (method)dictedit_newpatcherview,		"newpatcherview",				A_CANT, 0);
	class_addmethod(c, (method)dictedit_freepatcherview,	"freepatcherview",				A_CANT, 0);
	class_addmethod(c, (method)dictedit_notify,				"notify",						A_CANT,	0);
	class_addmethod(c, (method)object_obex_dumpout,			"dumpout",						A_CANT, 0);
	class_addmethod(c, (method)dictedit_toggle_component,	"dictedit_toggle_component",	A_CANT, 0);
	
	class_addmethod(c, (method)dictedit_menu_component,		"dictedit_menu_component",		A_CANT, 0);
	class_addmethod(c, (method)dictedit_text_component,		"dictedit_text_component",		A_CANT, 0);
	class_addmethod(c, (method)dictedit_integer_component,	"dictedit_integer_component",	A_CANT, 0);
	class_addmethod(c, (method)dictedit_getcellmenu,		"getcellmenu",					A_CANT, 0);

	CLASS_ATTR_SYM(c,			"dictionary",	0, t_dictedit, d_name);
	CLASS_ATTR_ACCESSORS(c,		"dictionary",	NULL, dictedit_set_dictionary);
	
	class_register(CLASS_BOX, c);
	s_dictedit_class = c;
	
	ps_nothing = gensym("");
	ps_modified = gensym("modified");
	ps_name = gensym("name");
	ps_type = gensym("type");
}


/************************************************************************************/
// Object Creation Method

void *dictedit_new(t_symbol *s, short argc, t_atom *argv) {
	t_dictedit*		x;
	t_dictionary*	d = NULL;

	if (!(d=object_dictionaryarg(argc, argv)))
		return NULL;

	x = (t_dictedit*)object_alloc(s_dictedit_class);
	if (x) {
		long flags = 0
				| JBOX_DRAWFIRSTIN
				| JBOX_NODRAWBOX
				| JBOX_DRAWINLAST
				//		| JBOX_TRANSPARENT
				//		| JBOX_NOGROW
				//		| JBOX_GROWY
				| JBOX_GROWBOTH
				//		| JBOX_IGNORELOCKCLICK
				| JBOX_HILITE
				//		| JBOX_BACKGROUND
				//		| JBOX_NOFLOATINSPECTOR
				//		| JBOX_TEXTFIELD
				;

		jbox_new(&x->d_box, flags, argc, argv);
		x->d_name = ps_nothing;
		x->d_box.b_firstin = (t_object *)x;
		x->d_outlet = outlet_new(x, NULL);

		x->d_columns = hashtab_new(13);
		hashtab_flags(x->d_columns, OBJ_FLAG_DATA);

		dictedit_initdataview(x);
		attr_dictionary_process(x, d);

		jbox_ready(&x->d_box);
	}
	return x;
}


void dictedit_initdataview(t_dictedit *x) {
	x->d_dataview = (t_object*)jdataview_new();
	jdataview_setclient(x->d_dataview, (t_object*)x);
	jdataview_setcolumnheaderheight(x->d_dataview, 40);
	jdataview_setheight(x->d_dataview, 16);
}


void dictedit_free(t_dictedit* x) {
	jbox_free(&x->d_box);
	object_free(x->d_dataview);
	
	if (x->d_dict)
		dictobj_release(x->d_dict);

}


void dictedit_newpatcherview(t_dictedit* x, t_object* patcherview) {
	jdataview_patchervis(x->d_dataview, patcherview, (t_object*)x);
}

void dictedit_freepatcherview(t_dictedit *x, t_object *patcherview) {
	jdataview_patcherinvis(x->d_dataview, patcherview);
}


t_max_err dictedit_notify(t_dictedit *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	if (sender == x->d_dict && msg == ps_modified) {
		dictedit_bang(x);
		return MAX_ERR_NONE;
	}
	else
		return jbox_notify((t_jbox *)x, s, msg, sender, data);
}



t_symbol* dictedit_getkey(t_dictedit *x, t_rowref rr) {
	return (t_symbol*)rr;
}


t_dictedit_column* dictedit_getcolumn(t_dictedit *x, t_symbol *colname) {
	t_dictedit_column*	dcolumn = nullptr;
	t_max_err			err = hashtab_lookup(x->d_columns, colname, (t_object**)&dcolumn);
	
	if (err)
		return nullptr;
	else
		return dcolumn;
}


/************************************************************************************/
// Methods bound to input/inlets


void dictedit_toggle_component(t_object *x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label) {
	*comptype = JCOLUMN_COMPONENT_CHECKBOX;
}


void dictedit_menu_component(t_object* self, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label) {
	*comptype = JCOLUMN_COMPONENT_MENU;
	*options = /*JCOLUMN_MENU_INDEX |*/ JCOLUMN_MENU_SELECT;
}


void dictedit_text_component(t_object* x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label) {
	*comptype = JCOLUMN_COMPONENT_TEXTEDITOR;
	*options = JCOLUMN_TEXT_ONESYMBOL;
}


void dictedit_integer_component(t_object* x, t_symbol *colname, t_rowref rr, long *comptype, long *options, t_symbol **label) {
	*comptype = JCOLUMN_COMPONENT_TEXTEDITOR;
	// *options = JCOLUMN_TEXT_INT;
	// the above doesn't work, causes memory corruption when scrolling the number outside of the inspector context
}


void dictedit_bang(t_dictedit *x) {
	jdataview_clear(x->d_dataview);
	
	t_atom*		names = NULL;
	long		namecount = 0;
	t_atom*		types = NULL;
	long		typecount = 0;
	t_atom*		widths = NULL;
	long		widthcount = 0; // TODO: leaking!
	
	dictionary_getatoms(x->d_dict, ps_name, &namecount, &names);
	dictionary_getatoms(x->d_dict, ps_type, &typecount, &types);
	dictionary_getatoms(x->d_dict, gensym("width"), &widthcount, &widths);
	
	for (auto i=0; i<namecount; ++i) {
//		t_atom_long			column_index = 0;
		t_dictedit_column*	dcolumn = nullptr;
		//t_max_err			err = hashtab_lookuplong(x->d_columns, atom_getsym(names+i), &column_index);
		t_max_err			err = hashtab_lookup(x->d_columns, atom_getsym(names+i), (t_object**)&dcolumn);
		
		if (!err) {
			// column already exists, so we leave it alone
		}
		else {
			dcolumn = new t_dictedit_column;
			dcolumn->name = atom_getsym(names+i);
//			dcolumn->type =
			dcolumn->use_range = false;
			dcolumn->index = i;
			dcolumn->dataview_column = jdataview_addcolumn(x->d_dataview, dcolumn->name, NULL, true);

			jcolumn_setlabel(dcolumn->dataview_column, atom_getsym(names+i));

			if (atomisatomarray(types+i)) {
				dcolumn->type = gensym("enum");
				jcolumn_setrowcomponentmsg(dcolumn->dataview_column, gensym("dictedit_menu_component"));
				jcolumn_setvaluemsg(dcolumn->dataview_column, gensym("setvalue"), nullptr, nullptr);
			}
			else if (atomisdictionary(types+i)) {
				t_dictionary*		d = (t_dictionary*)atom_getobj(types+i);
				long				keycount = 0;
				t_symbol**			keys = nullptr;
				
				dictionary_getkeys(d, &keycount, &keys);
				if (keycount) {
					t_symbol* type = keys[0];
					
					if (type == gensym("integer")) {
						long	range_ac = 0;
						t_atom*	range_av = nullptr;

						dictionary_getatoms(d, type, &range_ac, &range_av);
						
						if (range_ac == 2) {
							dcolumn->range[0] = atom_getlong(range_av+0);
							dcolumn->range[1] = atom_getlong(range_av+1);
							dcolumn->use_range = true;
						}

						// warning: duplicating code from below
						dcolumn->type = type;
						//jcolumn_setnumeric(column, true);
						jcolumn_setrowcomponentmsg(dcolumn->dataview_column, gensym("dictedit_integer_component"));
						jcolumn_setvaluemsg(dcolumn->dataview_column, gensym("setvalue"),NULL,NULL);
					}
					sysmem_freeptr(keys);
				}
			}
			else {
				if (atom_getsym(types+i) == gensym("toggle")) {
					dcolumn->type = atom_getsym(types+i);
					jcolumn_setcheckbox(dcolumn->dataview_column, gensym("setvalue"));	// this sets numeric
					jcolumn_setrowcomponentmsg(dcolumn->dataview_column, gensym("dictedit_toggle_component"));
				}
				else if (atom_getsym(types+i) == gensym("text")) {
					dcolumn->type = atom_getsym(types+i);
					jcolumn_setrowcomponentmsg(dcolumn->dataview_column, gensym("dictedit_text_component"));
					jcolumn_setvaluemsg(dcolumn->dataview_column, gensym("setvalue"),NULL,NULL);
				}
				else if (atom_getsym(types+i) == gensym("integer")) {
					dcolumn->type = atom_getsym(types+i);
					//jcolumn_setnumeric(column, true);
					jcolumn_setrowcomponentmsg(dcolumn->dataview_column, gensym("dictedit_integer_component"));
					jcolumn_setvaluemsg(dcolumn->dataview_column, gensym("setvalue"),NULL,NULL);
				}
				else {
					dcolumn->type = gensym("static");
				}
			}
			
			if (widthcount)
				jcolumn_setwidth(dcolumn->dataview_column, (long)atom_getlong(widths+i));
			
			hashtab_store(x->d_columns, atom_getsym(names+i), (t_object*)dcolumn);

		}
	}

	long		keycount = 0;
	t_symbol**	keys = nullptr;
	
	dictionary_getkeys_ordered(x->d_dict, &keycount, &keys);

	t_rowref*	rowrefs = (t_rowref*)malloc(sizeof(t_rowref) * keycount);
	int			j = 0;
	
	for (auto i=0; i<keycount; ++i) {
		t_symbol* key = *(keys+i);
		
		if (key == ps_name || key == ps_type || key == gensym("width"))
			continue;
		
		rowrefs[j] = (t_rowref*)key;
		++j;
	}
	
	jdataview_addrows(x->d_dataview, j, rowrefs);
	free(rowrefs);
}


void dictedit_getcelltext(t_dictedit *x, t_symbol *colname, t_rowref rr, char *text, long maxlen) {
	t_max_err	err;
	long		ac = 0;
	t_atom*		av = NULL;
	t_symbol*	key = dictedit_getkey(x, rr);
	auto		dcolumn = dictedit_getcolumn(x, colname);
	
	err = dictionary_getatoms(x->d_dict, key, &ac, &av);
	if (!err) {
		long	textsize = 0;
		char*	itemtext = nullptr;
		
		atom_gettext(1, av+(dcolumn->index), &textsize, &itemtext, OBEX_UTIL_ATOM_GETTEXT_SYM_NO_QUOTE);
		
		if (itemtext && itemtext[0]) {
			strncpy(text, itemtext, maxlen-1);
			sysmem_freeptr(itemtext);
		}
	}
}


void dictedit_getcellvalue(t_dictedit *x, t_symbol *colname, t_rowref rr, long *argc, t_atom *argv) {
	t_max_err	err;
	long		ac = 0;
	t_atom*		av = NULL;
	t_symbol*	key = dictedit_getkey(x, rr);
	auto		dcolumn = dictedit_getcolumn(x, colname);
	
	*argc = 1;
	
	err = dictionary_getatoms(x->d_dict, key, &ac, &av);

	if (!err)
		*argv = *(av+(dcolumn->index));
}


void dictedit_getcellmenu(t_dictedit *x, t_symbol *colname, t_rowref rr, long *argc, t_atom *argv, char **enabled, long *currentitemindex)
{
	auto			dcolumn = dictedit_getcolumn(x, colname);
	t_atom*			types = NULL;
	long			typecount = 0;
	
	dictionary_getatoms(x->d_dict, ps_type, &typecount, &types);
	
	t_atomarray*	aa = (t_atomarray*)atom_getobj(types+(dcolumn->index));
	long			ac = 0;
	t_atom*			av = nullptr;
	
	atomarray_getatoms(aa, &ac, &av);
	
	*argc = ac;
	*enabled = sysmem_newptr(ac);
	for (auto i=0; i<ac; ++i) {
		if (atomisstring(av+i))
			atom_setsym(argv+i, gensym(string_getptr((t_string*)atom_getobj(av+i))));
		else
			*(argv+i) =*(av+i);
		(*enabled)[i] = true;
	}
}


void dictedit_setvalue(t_dictedit *x, t_symbol *colname, t_rowref rr, long argc, t_atom *argv) {
	t_max_err	err;
	long		ac = 0;
	t_atom*		av = NULL;
	t_symbol*	key = dictedit_getkey(x, rr);
	auto		dcolumn = dictedit_getcolumn(x, colname);
	
	err = dictionary_getatoms(x->d_dict, key, &ac, &av);
	
	if (!err) {
		if (dcolumn->type == gensym("integer")) {
			auto val = atom_getlong(argv);
			
			if (dcolumn->use_range) {
				if (val < dcolumn->range[0])
					val = dcolumn->range[0];
				else if (val > dcolumn->range[1])
					val = dcolumn->range[1];
			}
			atom_setlong(av+(dcolumn->index), val);
		}
		else
			*(av+(dcolumn->index)) = *argv;
	}
	object_notify(x->d_dict, ps_modified, nullptr);
}


void dictedit_selectedrow(t_dictedit* self, t_rowref* rr) {
	outlet_anything(self->d_outlet, (t_symbol*)rr, 0, nullptr);
}


t_max_err dictedit_set_dictionary(t_dictedit *x, void *attr, long argc, t_atom *argv) {
	t_symbol* name = atom_getsym(argv);
	if (!name || name == ps_nothing) {
		object_error((t_object *)x, "invalid name specified", name);
		return MAX_ERR_GENERIC;
	}
	
	t_dictionary* d = dictobj_findregistered_retain(name);
	if (!d) {
		object_error((t_object *)x, "unable to reference dictionary named %s", name);
		return MAX_ERR_GENERIC;
	}
	
	if (x->d_dict) {
		object_detach_byptr(x, x->d_dict);
		dictobj_release(x->d_dict);
	}
	x->d_dict = d;
	x->d_name = name;
	object_attach_byptr(x, d);
	
	return MAX_ERR_NONE;
}