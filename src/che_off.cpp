#include "che_off.h"

#include <fstream>
#include <vector>
#include <cstring>
#include <cassert>

che_off::che_off(const vertex * vertices, const size_t & n_v, const index_t * faces, const size_t & n_f)
{
	init(vertices, n_v, faces, n_f);
}

che_off::che_off(const size_t & n_v, const size_t & n_f)
{
	init(n_v, n_f);
}

che_off::che_off(const string & file)
{
	init(file);
}

che_off::~che_off()
{

}

void che_off::read_file(const string & file)
{
	string soff;
	size_t n_v, n_f, v;

	ifstream is(file);

	assert(is.good());

	is >> soff;
	is >> n_v >> n_f >> v;
	init(n_v, n_f);

	int r, g, b, a;
	for(index_t i = 0; i < n_vertices_; i++)
	{
		is >> GT[i];
		if(soff[0] == 'C') // COFF file, ignore RGBA
			is >> r >> g >> b >> a;
	}

	index_t he = 0;
	for(index_t i = 0; i < n_faces_; i++)
	{
		is>>v;
		if(!i && v > che::P)
		{
			vertex * tGT = GT; GT = NULL;

			delete_me();
			init(n_v, n_f * (v - che::P + 1));

			GT = tGT;
		}

		for(index_t j = 0; j < v; j++)
			is>>VT[he++];

		// divide face
		if(v > che::P)
		{
			VT[he++] = VT[he - v - 1];
			VT[he++] = VT[he - v];

			i += (v - che::P);
		}
	}

	is.close();
}

void che_off::write_file(const string & file) const
{
	ofstream os(file);

	os << "OFF" << endl;
	os << n_vertices_ << " " << n_faces_ << " 0" << endl;

	for(size_t v = 0; v < n_vertices_; v++)
		os << GT[v] << endl;

	for(index_t he = 0; he < n_half_edges_; he++)
	{
		if(!(he % che::P)) os << che::P;
		os << " " << VT[he];
		if(he % che::P == che::P - 1) os << endl;
	}

	os.close();
}

