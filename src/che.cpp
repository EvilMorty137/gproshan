#include "che.h"

#include <cstring>
#include <cmath>
#include <cassert>
#include <set>

#include "viewer/viewer.h"

#include "include_arma.h"

index_t trig(const index_t & he)
{
	if(he == NIL) return NIL;
	return he / che::P;
}

index_t next(const index_t & he)
{
	if(he == NIL) return NIL;
	return che::P * trig(he) + (he + 1) % che::P;
}

index_t prev(const index_t & he)
{
	if(he == NIL) return NIL;
	return che::P * trig(he) + (he + che::P - 1) % che::P;
}

CHE::CHE(che * mesh)
{
	n_vertices = mesh->n_vertices_;
	n_faces = mesh->n_faces_;
	n_half_edges = mesh->n_half_edges_;

	GT = (vertex_cu *) mesh->GT;
	VT = mesh->VT;
	OT = mesh->OT;
	EVT = mesh->EVT;
}

che::~che()
{
	delete_me();
}

void che::star(star_t & s, const index_t & v)
{
	if(v >= n_vertices_) return;

	for_star(he, this, v)
		s.push_back(he);
}

void che::link(link_t & l, const index_t & v)
{
	if(v >= n_vertices_) return;

	for_star(he, this, v)
	{
		l.push_back(next(he));
		if(OT[prev(he)] == NIL)
			l.push_back(prev(he));
	}
}

void che::border(vector<index_t> & border, const index_t & b)
{
	for_border(he, this, BT[b])
		border.push_back(VT[he]);
}

bool che::is_border_v(const index_t & v) const
{
	assert(EVT[v] < n_half_edges_);
	return OT[EVT[v]] == NIL;
}

bool che::is_border_e(const index_t & e) const
{
	return OT[ET[e]] == NIL;
}

void che::flip(const index_t & e)
{
	index_t ha = ET[e];
	index_t hb = OT[ha];

	if(hb == NIL)
		return;

	index_t va = VT[ha];
	index_t vb = VT[hb];
	index_t vc = VT[prev(ha)];
	index_t vd = VT[prev(hb)];

	VT[prev(ha)] = vb;
	VT[ha] = vc;
	VT[next(ha)] = vd;
	VT[prev(hb)] = va;
	VT[hb] = vd;
	VT[next(hb)] = vc;

	index_t ot_pa = OT[prev(ha)];
	index_t ot_na = OT[next(ha)];
	index_t ot_pb = OT[prev(hb)];
	index_t ot_nb = OT[next(hb)];

	if(ot_pa != NIL) OT[ot_pa] = next(hb);
	if(ot_na != NIL) OT[ot_na] = prev(ha);
	if(ot_pb != NIL) OT[ot_pb] = next(ha);
	if(ot_nb != NIL) OT[ot_nb] = prev(hb);

	OT[prev(ha)] = ot_na;
	OT[next(ha)] = ot_pb;
	OT[prev(hb)] = ot_nb;
	OT[next(hb)] = ot_pa;

	if(EVT[va] == next(hb) || EVT[va] == ha) EVT[va] = prev(hb);
	if(EVT[vb] == next(ha) || EVT[vb] == hb) EVT[vb] = prev(ha);
	if(EVT[vc] == prev(ha)) EVT[vc] = next(hb);
	if(EVT[vd] == prev(hb)) EVT[vd] = next(ha);

	index_t e_pa = EHT[prev(ha)];
	index_t e_na = EHT[next(ha)];
	index_t e_pb = EHT[prev(hb)];
	index_t e_nb = EHT[next(hb)];

	ET[e_pa] = next(hb);
	ET[e_na] = prev(ha);
	ET[e_pb] = next(ha);
	ET[e_nb] = prev(hb);

	EHT[ET[e_pa]] = e_pa;
	if(OT[ET[e_pa]] != NIL) EHT[OT[ET[e_pa]]] = e_pa;
	EHT[ET[e_na]] = e_na;
	if(OT[ET[e_na]] != NIL) EHT[OT[ET[e_na]]] = e_na;
	EHT[ET[e_pb]] = e_pb;
	if(OT[ET[e_pb]] != NIL) EHT[OT[ET[e_pb]]] = e_pb;
	EHT[ET[e_nb]] = e_nb;
	if(OT[ET[e_nb]] != NIL) EHT[OT[ET[e_nb]]] = e_nb;
}

// https://www.mathworks.com/help/pde/ug/pdetriq.html
// 4*sqrt(3)*a
// q = ----------------
// h1^2+h2^2+h3^2
real_t che::pdetriq(const index_t & t) const
{
	index_t he = t * P;
	real_t h[3] = {
						*(GT[VT[next(he)]] - GT[VT[he]]),
						*(GT[VT[prev(he)]] - GT[VT[next(he)]]),
						*(GT[VT[he]] - GT[VT[prev(he)]])
					};
	return (4 * sqrt(3) * area_trig(t)) / (h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
}

percent_t che::quality()
{
	percent_t q = 0;

	#pragma omp parallel for reduction(+: q)
	for(index_t t = 0; t < n_faces_; t++)
		q += pdetriq(t) > 0.6; //is confederating good triangle

	return q * 100.0 / n_faces_;
}

area_t che::area_trig(const index_t & t) const
{
	index_t he = t * P;
	vertex a = GT[VT[next(he)]] - GT[VT[he]];
	vertex b = GT[VT[prev(he)]] - GT[VT[he]];

	return *(a * b) / 2;
}

area_t che::area_vertex(const index_t & v)
{
	area_t area_star = 0;
	for_star(he, this, v)
		area_star += area_trig(trig(he));

	return area_star / 3;
}

area_t che::area_surface() const
{
	area_t area = 0;

	#pragma omp parallel for reduction(+: area)
	for(index_t i = 0; i < n_faces_; i++)
		area += area_trig(i);

	return area;
}

vertex che::normal_he(const index_t & he) const
{
	vertex n = (GT[VT[next(he)]] - GT[VT[he]]) * (GT[VT[prev(he)]] - GT[VT[he]]);
	return n / *n;
}

vertex che::normal(const index_t & v)
{
	vertex n;
	area_t area, area_star = 0;

	for_star(he, this, v)
	{
		area = area_trig(trig(he));
		area_star += area;
		n += area * normal_he(he);
	}

	n /= area_star;
	return n / *n;
}

vertex che::gradient_he(const index_t & he, const distance_t *const & f) const
{
	index_t i = VT[he];
	index_t j = VT[next(he)];
	index_t k = VT[prev(he)];

	vertex xi = GT[i];
	vertex xj = GT[j];
	vertex xk = GT[k];

	vertex n = normal_he(he);

	area_t A2 = area_trig(trig(he)) * 2;

	vertex pij = n * (xj - xi);
	vertex pjk = n * (xk - xj);
	vertex pki = n * (xi - xk);

	vertex g = ( f[i] * pjk + f[j] * pki + f[k] * pij ) / A2;
	return g / *g;
}

vertex che::gradient(const index_t & v, const distance_t *const & f)
{
	vertex g;
	area_t area, area_star = 0;

	for_star(he, this, v)
	{
		area = area_trig(trig(he));
		area_star += area;
		g += area * gradient_he(he, f);
	}

	return g / area_star;
}

vertex che::barycenter(const index_t & t) const
{
	vertex bc;
	index_t tmp, he = t * P;
	tmp = he;

	do
	{
		bc += GT[VT[he]];
		he = next(he);
	}
	while(he != tmp);

	return bc / P;
}

vertex che::corr_vertex(corr_t & corr) const
{
	index_t he = corr.t * P;
	assert(he < n_half_edges_);
	return corr.alpha[0] * gt_vt(he) + corr.alpha[1] * gt_vt(next(he)) + corr.alpha[2] * gt_vt(prev(he));
}

real_t che::cotan(const index_t & he) const
{
	if(he == NIL) return 0;

	vertex a = GT[VT[he]] - GT[VT[prev(he)]];
	vertex b = GT[VT[next(he)]] - GT[VT[prev(he)]];

	return (a, b) / *(a * b);
}

real_t che::mean_edge() const
{
	real_t m = 0;

	#pragma omp parallel for reduction(+: m)
	for(index_t e = 0; e < n_edges_; e++)
		m += *(GT[VT[ET[e]]] - GT[VT[next(ET[e])]]);

	return m / n_edges_;
}

size_t che::memory() const
{
	return sizeof(*this) + n_vertices_ * (sizeof(vertex) + sizeof(index_t)) + filename_.size()
						+ sizeof(index_t) * (3 * n_half_edges_ + n_edges_ + n_borders_);
}

size_t che::genus() const
{
	size_t g = n_vertices_ - n_edges_ + n_faces_;
	return (g - 2) / (-2);
}

void che::normalize()
{
	vertex center;

	#pragma omp parallel for
	for(index_t v = 0; v < n_vertices_; v++)
	{
		#pragma omp critical
		center += GT[v];
	}

	center /= n_vertices_;

	real_t max_norm = 0;

	#pragma omp parallel for reduction(std::max : max_norm)
	for(index_t v = 0; v < n_vertices_; v++)
	{
		GT[v] -= center;
		max_norm = std::max(max_norm, *GT[v]);
	}

	#pragma omp parallel for
	for(index_t v = 0; v < n_vertices_; v++)
		GT[v] /= max_norm;
}

bool che::is_manifold() const
{
	return manifold;
}

const index_t & che::vt(const index_t & he) const
{
	assert(he < n_half_edges_);
	return VT[he];
}

const vertex & che::gt(const index_t & v) const
{
	assert(v < n_vertices_);
	return GT[v];
}

const vertex & che::gt_vt(const index_t & he) const
{
	assert(he < n_half_edges_);
	return GT[VT[he]];
}

const vertex & che::gt_vt_next_evt(const index_t & v) const
{
	assert(v < n_vertices_);
	return GT[VT[next(EVT[v])]];
}

const vertex & che::gt_e(const index_t & e, const bool & op)
{
	assert(e < n_edges_);
	return op ? GT[VT[next(ET[e])]] : GT[VT[ET[e]]];
}

const index_t & che::vt_e(const index_t & e, const bool & op)
{
	assert(e < n_edges_);
	return op ? VT[next(ET[e])] : VT[ET[e]];
}

const index_t & che::et(const index_t & e) const
{
	assert(e < n_edges_);
	return ET[e];
}

const index_t & che::ot_et(const index_t & e) const
{
	assert(e < n_edges_);
	return OT[ET[e]];
}

const index_t & che::ot(const index_t & he) const
{
	assert(he < n_half_edges_);
	return OT[he];
}

const index_t & che::ot_evt(const index_t & v) const
{
	assert(v < n_vertices_);
	return OT[EVT[v]];
}

const index_t & che::evt(const index_t & v) const
{
	assert(v < n_vertices_);
	return EVT[v];
}

const index_t & che::bt(const index_t & b) const
{
	return BT[b];
}

const size_t & che::n_vertices() const
{
	return n_vertices_;
}

const size_t & che::n_faces() const
{
	return n_faces_;
}

const size_t & che::n_half_edges() const
{
	return n_half_edges_;
}

const size_t & che::n_edges() const
{
	return n_edges_;
}

const size_t & che::n_borders() const
{
	return n_borders_;
}

size_t che::max_degree() const
{
	size_t d, md = 0;

	#pragma omp parallel for private(d) reduction(max: md)
	for(index_t v = 0; v < n_vertices_; v++)
	{
		d = 0;
		for_star(he, this, v) d++;
		d += is_border_v(v);
		md = max(md, d);
	}

	return md;
}

vertex & che::get_vertex(index_t v)
{
	return GT[v];
}

void che::set_vertices(const vertex *const& positions, size_t n, const index_t & v_i)
{
	if(!positions) return;
	if(!n) n = n_vertices_;
	memcpy(GT + v_i, positions, sizeof(vertex) * n);
}

const string & che::filename() const
{
	return filename_;
}

const string che::filename_size() const
{
	return filename_ + "_" + to_string(n_vertices_);
}

void che::set_filename(const string & f)
{
	filename_ = f;
}

const string che::name() const
{
	index_t p = filename_.find_last_of('/');
	index_t q = filename_.find_last_of('.');
	return filename_.substr(p + 1, q - p - 1);
}

const string che::name_size() const
{
	return name() + "_" + to_string(n_vertices_);
}

void che::reload()
{
	delete_me();
	init(filename_);
}

void che::compute_toplesets(index_t *& rings, index_t *& sorted, vector<index_t> & limites, const vector<index_t> & sources, const index_t & k)
{
	if(!sources.size()) return;

	index_t v;
	memset(rings, 255, sizeof(index_t) * n_vertices_);

	index_t r = 0;
	index_t p = 0;
	for(; p < sources.size(); p++)
	{
		sorted[p] = sources[p];
		rings[sources[p]] = r;
	}

	limites.push_back(0);
	index_t s = 0;
	for(; s < p; s++)
	{
		v = sorted[s];

		if(rings[v] != NIL && rings[v] > r)
		{
			r++;

			if(r > k) break;
			limites.push_back(s);
		}

		link_t v_link;
		link(v_link, v);
		for(index_t he: v_link)
		{
			if(rings[VT[he]] == NIL)
			{
				rings[VT[he]] = rings[v] + 1;
				sorted[p++] = VT[he];
			}
		}

	}

	limites.push_back(s);
}

void che::multiplicate_vertices()
{
	size_t nv = n_vertices_ + n_faces_;
	size_t nf = 3 * n_faces_;
	size_t nh = 3 * n_half_edges_;
	size_t ne = n_edges_ + n_faces_ * 3;

	vertex * aGT = new vertex[nv];
	index_t * aVT = new index_t[nh];
	index_t * aOT = new index_t[nh];
	index_t * aEVT = new index_t[nv];
	index_t * aET = new index_t[ne];
	index_t * aEHT = new index_t[nh];

	memcpy(aGT, GT, n_vertices_ * sizeof(vertex));

	#pragma omp parallel for
	for(index_t e = 0; e < n_edges_; e++)
		aET[e] = ET[e] * 3;

	#pragma omp parallel for
	for(index_t v = 0; v < n_vertices_; v++)
		aEVT[v] = EVT[v] * 3;

	#pragma omp parallel for
	for(index_t f = 0; f < n_faces_; f++)
	{
		index_t he = f * P;
		index_t v = n_vertices_ + f;
		aGT[v] = (GT[VT[prev(he)]] + GT[VT[he]] + GT[VT[next(he)]]) / 3;

		index_t ahe = f * P * 3;
		aVT[ahe] = aVT[ahe + 7] = VT[he];
		aVT[ahe + 3] = aVT[ahe + 1] = VT[next(he)];
		aVT[ahe + 6] = aVT[ahe + 4] = VT[prev(he)];
		aVT[ahe + 2] = aVT[ahe + 5] = aVT[ahe + 8] = v;

		aOT[ahe] = OT[he] != NIL ? OT[he] * 3 : NIL;
		aOT[ahe + 3] = OT[he + 1] != NIL ? OT[he + 1] * 3 : NIL;
		aOT[ahe + 6] = OT[he + 2] != NIL ? OT[he + 2] * 3 : NIL;

		aOT[ahe + 1] = ahe + 5;
		aOT[ahe + 2] = ahe + 7;
		aOT[ahe + 4] = ahe + 8;
		aOT[ahe + 5] = ahe + 1;
		aOT[ahe + 8] = ahe + 4;
		aOT[ahe + 7] = ahe + 2;

		aEVT[v] = ahe + 2;

		aET[n_edges_ + he] = ahe + 2;
		aET[n_edges_ + he + 1] = ahe + 5;
		aET[n_edges_ + he + 2] = ahe + 8;
	}

	delete_me();
	GT = aGT;
	VT = aVT;
	OT = aOT;
	EVT = aEVT;
	ET = aET;
	EHT = aEHT;

	size_t n_flips = n_edges_;
	n_vertices_ = nv;
	n_faces_ = nf;
	n_half_edges_ = nh;
	n_edges_ = ne;

	update_eht();
	update_bt();

	for(index_t e = 0; e < n_flips; e++)
		flip(e);
}

void che::remove_non_manifold_vertices()
{
	for( index_t he = 0; he < n_half_edges_; he+=3)
	{
		if(EVT[ VT[he] ] == NIL || EVT[ VT[he+1]] == NIL || EVT[ VT[he+2] ] == NIL)
		{
			VT[he] = NIL;
			VT[he + 1 ] = NIL;
			VT[he + 2 ] = NIL;
		}
	}

	/* save in vectors */
	vector<vertex> new_vertices;
	vector<index_t> removed;
	vector<index_t> new_faces; // each 3

	debug_me(removing vertex);
	for(index_t v = 0; v < n_vertices_; v++)
	{
		if(EVT[v] != NIL)
			new_vertices.push_back(GT[v]);
		else
			removed.push_back(v);
	}
	removed.push_back(n_vertices_);

	debug(removed.size())
	debug(removed[0])
	index_t r = 1;
	index_t d = 1;
	for(index_t v = removed[0] + 1; v < n_vertices_; v++)
	{
		if(v < removed[r])
		{
			for_star(he, this, v)
				if(VT[he] != NIL) VT[he] = v - d;
		}
		else if(v == removed[r])
		{
			d++;
			r++;
		}
	}

	for(index_t he = 0; he < n_half_edges_; he++)
		if(VT[he] != NIL)
			new_faces.push_back(VT[he]);
		else { debug(he) }

	debug(new_vertices.size())
	debug(new_faces.size())
	debug_me(removing vertex);
	delete_me();
	debug_me(removing vertex);
	init(new_vertices.data(), new_vertices.size(), new_faces.data(), new_faces.size() / P);
	debug_me(removing vertex);
}

void che::remove_vertices(const vector<index_t> & vertices)
{
	if(!vertices.size()) return;

	debug_me(removing vertex);
	for(index_t v: vertices)
	{
		for_star(he, this, v)
		{
			VT[he] = NIL;
			VT[prev(he)] = NIL;
			VT[next(he)] = NIL;

			debug(he)
			debug(next(he))
			debug(prev(he))
		}

		debug("todos r")
		debug(EVT[v])
		EVT[v] = NIL;
	}
	/* save in vectors */
	vector<vertex> new_vertices;
	vector<index_t> removed;
	vector<index_t> new_faces; // each 3

	debug_me(removing vertex);
	for(index_t v = 0; v < n_vertices_; v++)
	{
		if(EVT[v] != NIL)
			new_vertices.push_back(GT[v]);
		else
			removed.push_back(v);
	}
	removed.push_back(n_vertices_);

	debug(removed.size())
	debug(removed[0])
	index_t r = 1;
	index_t d = 1;
	for(index_t v = removed[0] + 1; v < n_vertices_; v++)
	{
		if(v < removed[r])
		{
			for_star(he, this, v)
				if(VT[he] != NIL) VT[he] = v - d;
		}
		else if(v == removed[r])
		{
			d++;
			r++;
		}
	}

	for(index_t he = 0; he < n_half_edges_; he++)
		if(VT[he] != NIL)
			new_faces.push_back(VT[he]);
		else debug(he)

	debug(new_vertices.size())
	debug(new_faces.size())
	debug_me(removing vertex);
	delete_me();
	debug_me(removing vertex);
	init(new_vertices.data(), new_vertices.size(), new_faces.data(), new_faces.size() / P);
	debug_me(removing vertex);
}

void che::merge(const che * mesh, const vector<index_t> & com_vertices)
{
//	write_file("big.off");
//	mesh->write_file("small.off");
debug_me(fill holes)
	size_t ncv = com_vertices.size();
	bool is_open = mesh->VT[next(mesh->EVT[0])] >= ncv;
	debug(is_open)

	size_t nv = n_vertices_ + mesh->n_vertices_ - ncv;
	size_t nf = n_faces_ + mesh->n_faces_;
	size_t nh = n_half_edges_ + mesh->n_half_edges_;
	size_t ne = n_edges_ + mesh->n_edges_ - (ncv - is_open);

	vertex * aGT = new vertex[nv];
	index_t * aVT = new index_t[nh];
	index_t * aOT = new index_t[nh];
	index_t * aEVT = new index_t[nv];
	index_t * aET = new index_t[ne];
	index_t * aEHT = new index_t[nh];

debug_me(fill holes)
	memcpy(aGT, GT, sizeof(vertex) * n_vertices_);
	memcpy(aGT + n_vertices_, mesh->GT + ncv, sizeof(vertex) * (nv - n_vertices_));

	memcpy(aVT, VT, sizeof(index_t) * n_half_edges_);

	index_t * t_aVT = aVT + n_half_edges_;
	for(index_t he = 0; he < mesh->n_half_edges_; he++)
		t_aVT[he] = mesh->VT[he] < ncv ? com_vertices[mesh->VT[he]] : mesh->VT[he] + n_vertices_ - ncv;
debug_me(fill holes)

	memcpy(aOT, OT, sizeof(index_t) * n_half_edges_);
debug_me(fill holes)

	index_t * t_aOT = aOT + n_half_edges_;
	for(index_t he = 0; he < mesh->n_half_edges_; he++)
		t_aOT[he] = mesh->OT[he] != NIL ? mesh->OT[he] + n_half_edges_ : NIL;
debug_me(fill holes)

	for(index_t v, he_v, he_i, i = 0; i < ncv; i++)
	{
		he_i = mesh->EVT[i];
		if(he_i != NIL && mesh->VT[next(he_i)] < ncv)
		{
			v = com_vertices[mesh->VT[next(he_i)]];
			he_v = EVT[v];
			aOT[he_v] = he_i + n_half_edges_;
			aOT[aOT[he_v]] = he_v;
		}
	}

debug_me(fill holes)
	memcpy(aEVT, EVT, sizeof(index_t) * n_vertices_);
debug_me(fill holes)
	if(is_open)
		aEVT[com_vertices[0]] = mesh->EVT[0] != NIL ? mesh->EVT[0] + n_half_edges_ : NIL;

debug_me(fill holes)
	index_t * t_aEVT = aEVT + n_vertices_;
	for(index_t v = ncv; v < mesh->n_vertices_; v++)
		t_aEVT[v - ncv] = mesh->EVT[v] != NIL ? mesh->EVT[v] + n_half_edges_ : NIL;
debug_me(fill holes)

	memcpy(aET, ET, sizeof(index_t) * n_edges_);
debug_me(fill holes)

	bool * common_edge = new bool[mesh->n_edges_];
	memset(common_edge, 0, sizeof(bool) * mesh->n_edges_);
	for(index_t he_i, i = 0; i < ncv; i++)
	{
		he_i = mesh->EVT[i];
		if(he_i != NIL && mesh->VT[next(he_i)] < ncv)
			common_edge[mesh->EHT[he_i]] = true;
	}

	index_t ae = n_edges_;
	for(index_t e = 0; e < mesh->n_edges_; e++)
		if(!common_edge[e])
			aET[ae++] = mesh->ET[e] + n_half_edges_;

	debug(ae == ne)
	debug(ae)
	debug(ne)
	assert(ae == ne);
	delete [] common_edge;
debug_me(fill holes)
	delete_me();

debug_me(fill holes)
	GT = aGT;
	VT = aVT;
	OT = aOT;
	EVT = aEVT;
	ET = aET;
	EHT = aEHT;

	n_vertices_ = nv;
	n_faces_ = nf;
	n_half_edges_ = nh;
	n_edges_ = ne;

	debug_me(inpainting)
	update_eht();
	debug_me(inpainting)
	update_bt();
	debug_me(inpainting)
}

void che::set_head_vertices(index_t * head, const size_t & n)
{
	for(index_t v, i = 0; i < n; i++)
	{
		v = head[i];

		for(index_t j = i + 1; j < n; j++)
			if(i == head[j])
			{
				head[j] = v;
				break;
			}

		swap(GT[v], GT[i]);

		for_star(he, this, v)
			VT[he] = i;
		for_star(he, this, i)
			VT[he] = v;

		swap(EVT[v], EVT[i]);
		for(index_t b = 0; b < n_borders_; b++)
		{
			if(BT[b] == i) BT[b] = v;
			else if(BT[b] == v) BT[b] = i;
		}
	}
}

index_t che::link_intersect(const index_t & v_a, const index_t & v_b)
{
	index_t intersect = 0;

	link_t link_a, link_b;
	link(link_a, v_a);
	link(link_b, v_b);

	for(index_t & he_a: link_a)
	for(index_t & he_b: link_b)
		if(VT[he_a] == VT[he_b])
			intersect++;

	return intersect;
}

corr_t * che::edge_collapse(const index_t *const & sort_edges, const vertex *const & normals)
{
	if(n_faces_ < 2) return NULL;

	// init default corr
	corr_t * corr = new corr_t[n_vertices_];
	#pragma omp parallel for
	for(index_t v = 0; v < n_vertices_; v++)
		corr[v].init(EVT[v]);

	short * faces_fixed = new short[n_faces_];
	memset(faces_fixed, 0, sizeof(short) * n_faces_);
	index_t * deleted_vertices = new index_t[n_vertices_];
	memset(deleted_vertices, 0, sizeof(index_t) * n_vertices_);

	index_t e_d, he_d, ohe_d, va, vb;
	vertex aux_va, aux_vb;

	// update corr to vertex opposite to edge
	auto update_corr_v = [this, &corr](const index_t & he)
	{
		index_t v = VT[prev(he)];
		if(trig(EVT[v]) == trig(he))
		{
			EVT[v] = OT[next(he)];
			corr[v].init(EVT[v]);
		}
	};

	bool is_collapse;

	for(index_t e = 0; e < n_edges_; e++)
	{
		//e_d = ne;
		e_d = sort_edges ? sort_edges[e] : rand() % n_edges_;
		assert(e_d < n_edges_);

		he_d = ET[e_d];
		ohe_d = OT[he_d];
		va = VT[he_d];
		vb = VT[next(he_d)];

		//is_border_v(va) && is_border_v(vb) -> is_border_e(e_d)
		if( !faces_fixed[trig(he_d)] && (ohe_d != NIL ? !faces_fixed[trig(ohe_d)] : true) &&
			(!(is_border_v(va) && is_border_v(vb)) || is_border_e(e_d)) )
		{
			is_collapse = link_intersect(va, vb) == (1 + (ohe_d != NIL));

			if(is_collapse)
			for_star(he, this, va)
				if(faces_fixed[trig(he)])
				{
					is_collapse = false;
					break;
				}

			if(is_collapse)
			for_star(he, this, vb)
				if(faces_fixed[trig(he)])
				{
					is_collapse = false;
					break;
				}

			if(is_collapse)
			{
				update_corr_v(he_d);
				if(ohe_d != NIL)
					update_corr_v(ohe_d);

				for_star(he, this, va)
					if(!faces_fixed[trig(he)]) faces_fixed[trig(he)] = 1;

				for_star(he, this, vb)
					if(!faces_fixed[trig(he)]) faces_fixed[trig(he)] = 1;

				faces_fixed[trig(he_d)] = -1;
				if(ohe_d != NIL) faces_fixed[trig(ohe_d)] = -1;
				for_star(he, this, vb)
					VT[he] = va;

				deleted_vertices[vb] = 1;

				aux_va = GT[va];
				aux_vb = GT[vb];
				GT[va] = GT[vb] = (GT[va] + GT[vb]) / 2;

				vector<index_t> he_trigs;
				for_star(he, this, va)
				if(faces_fixed[trig(he)] > -1)
					he_trigs.push_back(trig(he) * P);
				for_star(he, this, vb)
				if(faces_fixed[trig(he)] > -1)
					he_trigs.push_back(trig(he) * P);

				debug(va)
				corr[va] = find_corr(aux_va, normals[va], he_trigs);
				debug(vb)
				corr[vb] = find_corr(aux_vb, normals[vb], he_trigs);

				EVT[vb] = NIL;
			}
		}
	}

	vector<vertex> new_vertices;
	vector<index_t> new_faces;
	new_vertices.reserve(n_vertices_);
	new_faces.reserve(n_faces_);

	index_t dv = 0;
	for(index_t v = 0; v < n_vertices_; v++)
	{
		if(deleted_vertices[v]) dv++;
		else
		{
			deleted_vertices[v] = dv;
			new_vertices.push_back(GT[v]);
		}
	}

	index_t * map_he = new index_t[n_half_edges_];
	memset(map_he, 255, sizeof(index_t) * n_half_edges_);

	for(index_t n_he = 0, he = 0; he < n_half_edges_; he++)
		if(faces_fixed[trig(he)] > -1)
		{
			new_faces.push_back(VT[he] - deleted_vertices[VT[he]]);
			map_he[he] = n_he++;
		}

	for(index_t v = 0; v < n_vertices_; v++)
		corr[v].t = trig(map_he[corr[v].t * P]);

	delete_me();
	init(new_vertices.data(), new_vertices.size(), new_faces.data(), new_faces.size() / P);

	delete [] faces_fixed;
	delete [] deleted_vertices;
	delete [] map_he;

	return corr;
}

corr_t che::find_corr(const vertex & v, const vertex & n, const vector<index_t> & he_trigs)
{
	distance_t d, dist = INFINITY;
	corr_t corr, corr_d;

	a_mat A(4, 4, arma::fill::ones);
	a_vec x(4);
	x(0) = v.x; x(1) = v.y; x(2) = v.z; x(3) = 1;

	a_vec alpha(&corr.alpha.x, 3, false, true);
	a_vec a;
	vertex aux;

	auto update_dist = [&]()
	{
		if(d < dist)
		{
			dist = d;
			corr_d = corr;
		}
	};

	for(const index_t & he: he_trigs)
	{
		corr.t = trig(he);

		aux = gt_vt(he);
		A(0, 0) = aux.x; A(1, 0) = aux.y; A(2, 0) = aux.z;

		aux = gt_vt(next(he));
		A(0, 1) = aux.x; A(1, 1) = aux.y; A(2, 1) = aux.z;

		aux = gt_vt(prev(he));
		A(0, 2) = aux.x; A(1, 2) = aux.y; A(2, 2) = aux.z;

		A(0, 3) = -n.x;
		A(1, 3) = -n.y;
		A(2, 3) = -n.z;
		A(3, 3) = 0;

		if(solve(a, A, x, arma::solve_opts::no_approx))
		{
			debug(a)
		if(all(a >= 0) && sum(a.head(3)) == 1)
		{
			alpha = a.head(3);
			d = 0;
			update_dist();
		}
		else
		{
			x = A.submat(0, 0, 3, 2) * alpha;
			auto dist_to_edge = [&](const index_t & i, const index_t & j)
			{
				a_mat B = A.cols(i, j);
				a = solve(B, x);
				d = norm(x - B * a);

				corr.alpha = 0;

				if(all(a >= 0) && sum(a.head(2)) == 1)
				{
					corr.alpha[i] = a(0);
					corr.alpha[j] = a(1);
					update_dist();
				}
				else
				{
					corr.alpha[i] = 1;
					d = norm(x - A.col(i));
					update_dist();

					corr.alpha[i] = 0;
					corr.alpha[j] = 1;
					d = norm(x - A.col(j));
					update_dist();
				}
			};

			dist_to_edge(0, 1);
			dist_to_edge(1, 2);
			dist_to_edge(0, 2);
		}
		}
	}

	if(corr_d.t == NIL)
	{
		debug(n)
		debug(x)
		debug(A)
	}
	return corr_d;
}

void che::init(const vertex * vertices, const index_t & n_v, const index_t * faces, const index_t & n_f)
{
	init(n_v, n_f);

	memcpy(GT, vertices, n_vertices_ * sizeof(vertex));
	memcpy(VT, faces, n_half_edges_ * sizeof(index_t));

	update_evt_ot_et();
	update_eht();
	update_bt();
}

void che::init(const string & file)
{
	filename_ = file;
	read_file(filename_);
	update_evt_ot_et();
	update_eht();
	update_bt();
}

void che::init(const size_t & n_v, const size_t & n_f)
{
	n_vertices_ = n_v;
	n_faces_ = n_f;
	n_half_edges_ = n_edges_ = n_borders_ = 0;

	GT = NULL;
	VT = OT = EVT = ET = BT = NULL;
	manifold = true;

	if(!n_vertices_ || !n_faces_)
	{
		filename_ = "";
		return;
	}

	n_half_edges_ = che::P * n_faces_;
	n_edges_ = 0; //n_half_edges_ / 2;	/**/

	GT = new vertex[n_vertices_];
	VT = new index_t[n_half_edges_];
	OT = new index_t[n_half_edges_];
	EVT = new index_t[n_vertices_];
	EHT = new index_t[n_half_edges_];
}

void che::update_evt_ot_et()
{
	vector<index_t> * he_p_vertex = new vector<index_t>[n_vertices_];

	memset(EVT, 255, sizeof(index_t) * n_vertices_);

	//vertex table
	for(index_t he = 0; he < n_half_edges_; he++)
	{
		EVT[VT[he]] = he;
		he_p_vertex[VT[he]].push_back(he);
	}

	//opposite table - edge table
	memset(OT, 255, sizeof(index_t) * n_half_edges_);

	vector<index_t> et;
	for(index_t he = 0; he < n_half_edges_; he++)
	{
		if(OT[he] == NIL)
		{
			et.push_back(he);
			for(index_t h: he_p_vertex[VT[he]])
			{
				if(VT[prev(h)] == VT[next(he)])
				{
					OT[he] = prev(h);
					OT[prev(h)] = he;
					break;
				}
			}
		}
		else assert(he == OT[OT[he]]);
	}

	//edge table
	n_edges_ = et.size();
	ET = new index_t[n_edges_];
	memcpy(ET, et.data(), sizeof(index_t) * n_edges_);


	for(index_t he = 0; he < n_half_edges_; he++)
		if(OT[he] == NIL && EVT[VT[he]] != NIL)
		{
			if(OT[EVT[VT[he]]] == NIL && EVT[VT[he]] != he)
			{
				manifold = false;
				EVT[VT[he]] = NIL;
			}
			else EVT[VT[he]] = he;
		}

	for(index_t v = 0; v < n_vertices_; v++)
		if(EVT[v] >= n_half_edges_)
		{
//			debug(EVT[v])
//			assert(EVT[v] < n_half_edges_);
		}

	delete [] he_p_vertex;
}

void che::update_eht()
{
	#pragma omp parallel for
	for(index_t e = 0; e < n_edges_; e++)
	{
		EHT[ET[e]] = e;
		if(OT[ET[e]] != NIL)
			EHT[OT[ET[e]]] = e;
	}
}

void che::update_bt()
{
	if(!manifold) return;

	bool * border = new bool[n_vertices_];
	memset(border, 0, sizeof(bool) * n_vertices_);

	vector<index_t> borders;

	for(index_t v = 0; v < n_vertices_; v++)
		if(!border[v] && EVT[v] != NIL && OT[EVT[v]] == NIL)
		{
			borders.push_back(v);
			for_border(he, this, v)
				border[VT[he]] = true;
		}

	n_borders_ = borders.size();
	if(n_borders_)
	{
		BT = new index_t[n_borders_];
		memcpy(BT, borders.data(), sizeof(index_t) * n_borders_);
	}
	else BT = NULL;

	delete [] border;
}

void che::delete_me()
{
	if(GT) delete [] GT;
	if(VT) delete [] VT;
	if(OT) delete [] OT;
	if(EVT) delete [] EVT;
	if(ET) delete [] ET;
	if(EHT) delete [] EHT;
	if(BT) delete [] BT;
}

