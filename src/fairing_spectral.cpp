#include "fairing_spectral.h"
#include "laplacian.h"

fairing_spectral::fairing_spectral(const size_t & k_): fairing(), k(k_)
{
}

fairing_spectral::~fairing_spectral()
{

}

void fairing_spectral::compute(che * shape)
{
	double time;

	a_sp_mat L, A;

	TIC(time) laplacian(shape, L, A); TOC(time)
	debug(time)

	positions = new vertex[shape->n_vertices()];

	a_mat X((real_t *) positions, 3, shape->n_vertices(), false, true);

	#pragma omp parallel for
	for(index_t v = 0; v < shape->n_vertices(); v++)
		positions[v] = shape->gt(v);

	a_vec eigval;
	a_mat eigvec;

	TIC(time) k = eigs_laplacian(eigval, eigvec, shape, L, A, k); TOC(time)
	debug(time)

	X = X * eigvec * eigvec.t();
}

