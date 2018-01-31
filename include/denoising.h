#ifndef DENOISING_H
#define DENOISING_H

#include "dictionary.h"

#include <armadillo>

using namespace arma;

class denoising : public dictionary
{
	public:
		denoising(che *const & _mesh, basis *const & _phi_basis, const size_t & _m, const size_t & _M, const distance_t & _f, const bool & _plot = false);
		virtual ~denoising();
        void execute();

};

#endif // DICTIONARY_H
