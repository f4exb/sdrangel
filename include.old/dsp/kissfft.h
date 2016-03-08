#ifndef INCLUDE_KISSFFT_H
#define INCLUDE_KISSFFT_H

#include <complex>
#include <vector>

/*
Copyright (c) 2003-2010 Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

	* Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.
	* Neither the author nor the names of any contributors may be used to
	endorse or promote products derived from this software without
	specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

namespace kissfft_utils {

	template<typename T_scalar, typename T_complex>
	struct traits {
		typedef T_scalar scalar_type;
		typedef T_complex cpx_type;
		void fill_twiddles(std::complex<T_scalar>* dst, int nfft, bool inverse)
		{
			T_scalar phinc = (inverse ? 2 : -2) * acos((T_scalar)-1) / nfft;
			for(int i = 0; i < nfft; ++i)
				dst[i] = exp(std::complex<T_scalar>(0, i * phinc));
		}

		void prepare(std::vector<std::complex<T_scalar> >& dst, int nfft, bool inverse, std::vector<int>& stageRadix, std::vector<int>& stageRemainder)
		{
			_twiddles.resize(nfft);
			fill_twiddles(&_twiddles[0], nfft, inverse);
			dst = _twiddles;

			//factorize
			//start factoring out 4's, then 2's, then 3,5,7,9,...
			int n = nfft;
			int p = 4;
			do {
				while(n % p) {
					switch(p) {
						case 4:
							p = 2;
							break;
						case 2:
							p = 3;
							break;
						default:
							p += 2;
							break;
					}
					if(p * p > n)
						p = n;// no more factors
				}
				n /= p;
				stageRadix.push_back(p);
				stageRemainder.push_back(n);
			} while(n > 1);
		}
		std::vector<cpx_type> _twiddles;

		const cpx_type twiddle(int i)
		{
			return _twiddles[i];
		}
	};

} // namespace

template<typename T_Scalar, typename T_Complex, typename T_traits = kissfft_utils::traits<T_Scalar, T_Complex> >
class kissfft {
public:
	typedef T_traits traits_type;
	typedef typename traits_type::scalar_type scalar_type;
	typedef typename traits_type::cpx_type cpx_type;

	kissfft()
	{
	}

	kissfft(int nfft, bool inverse, const traits_type & traits = traits_type()) :
		_nfft(nfft), _inverse(inverse), _traits(traits)
	{
		_traits.prepare(_twiddles, _nfft, _inverse, _stageRadix, _stageRemainder);
	}

	void configure(int nfft, bool inverse, const traits_type & traits = traits_type())
	{
		_twiddles.clear();
		_stageRadix.clear();
		_stageRemainder.clear();

		_nfft = nfft;
		_inverse = inverse;
		_traits = traits;
		_traits.prepare(_twiddles, _nfft, _inverse, _stageRadix, _stageRemainder);
	}

	void transform(const cpx_type* src, cpx_type* dst)
	{
		kf_work(0, dst, src, 1, 1);
	}

private:
	void kf_work(int stage, cpx_type* Fout, const cpx_type* f, size_t fstride, size_t in_stride)
	{
		int p = _stageRadix[stage];
		int m = _stageRemainder[stage];
		cpx_type * Fout_beg = Fout;
		cpx_type * Fout_end = Fout + p * m;

		if(m == 1) {
			do {
				*Fout = *f;
				f += fstride * in_stride;
			} while(++Fout != Fout_end);
		} else {
			do {
				// recursive call:
				// DFT of size m*p performed by doing
				// p instances of smaller DFTs of size m,
				// each one takes a decimated version of the input
				kf_work(stage + 1, Fout, f, fstride * p, in_stride);
				f += fstride * in_stride;
			} while((Fout += m) != Fout_end);
		}

		Fout = Fout_beg;

		// recombine the p smaller DFTs
		switch(p) {
			case 2:
				kf_bfly2(Fout, fstride, m);
				break;
			case 3:
				kf_bfly3(Fout, fstride, m);
				break;
			case 4:
				kf_bfly4(Fout, fstride, m);
				break;
			case 5:
				kf_bfly5(Fout, fstride, m);
				break;
			default:
				kf_bfly_generic(Fout, fstride, m, p);
				break;
		}
	}

	// these were #define macros in the original kiss_fft
	void C_ADD(cpx_type& c, const cpx_type& a, const cpx_type& b)
	{
		c = a + b;
	}
	void C_MUL(cpx_type& c, const cpx_type& a, const cpx_type& b)
	{
		//c = a * b;
		c = cpx_type(a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real());
	}
	void C_SUB(cpx_type& c, const cpx_type& a, const cpx_type& b)
	{
		c = a - b;
	}
	void C_ADDTO(cpx_type& c, const cpx_type& a)
	{
		c += a;
	}
	void C_FIXDIV(cpx_type&, int)
	{
	} // NO-OP for float types
	scalar_type S_MUL(const scalar_type& a, const scalar_type& b)
	{
		return a * b;
	}
	scalar_type HALF_OF(const scalar_type& a)
	{
		return a * .5;
	}
	void C_MULBYSCALAR(cpx_type& c, const scalar_type& a)
	{
		c *= a;
	}

	void kf_bfly2(cpx_type* Fout, const size_t fstride, int m)
	{
		for(int k = 0; k < m; ++k) {
			//cpx_type t = Fout[m + k] * _traits.twiddle(k * fstride);
			cpx_type t;
			C_MUL(t, Fout[m + k], _traits.twiddle(k * fstride));
			Fout[m + k] = Fout[k] - t;
			Fout[k] += t;
		}
	}

	void kf_bfly4(cpx_type* Fout, const size_t fstride, const size_t m)
	{
		cpx_type scratch[7];
		int negative_if_inverse = _inverse * -2 + 1;
		for(size_t k = 0; k < m; ++k) {
			//scratch[0] = Fout[k + m] * _traits.twiddle(k * fstride);
			C_MUL(scratch[0], Fout[k + m], _traits.twiddle(k * fstride));
			C_MUL(scratch[1], Fout[k + 2 * m], _traits.twiddle(k * fstride * 2));
			C_MUL(scratch[2], Fout[k + 3 * m], _traits.twiddle(k * fstride * 3));
			scratch[5] = Fout[k] - scratch[1];

			Fout[k] += scratch[1];
			scratch[3] = scratch[0] + scratch[2];
			scratch[4] = scratch[0] - scratch[2];
			scratch[4] = cpx_type(scratch[4].imag() * negative_if_inverse, -scratch[4].real() * negative_if_inverse);

			Fout[k + 2 * m] = Fout[k] - scratch[3];
			Fout[k] += scratch[3];
			Fout[k + m] = scratch[5] + scratch[4];
			Fout[k + 3 * m] = scratch[5] - scratch[4];
		}
	}

	void kf_bfly3(cpx_type* Fout, const size_t fstride, const size_t m)
	{
		size_t k = m;
		const size_t m2 = 2 * m;
		cpx_type* tw1;
		cpx_type* tw2;
		cpx_type scratch[5];
		cpx_type epi3;
		epi3 = _twiddles[fstride * m];
		tw1 = tw2 = &_twiddles[0];

		do {
			C_FIXDIV(*Fout, 3);
			C_FIXDIV(Fout[m], 3);
			C_FIXDIV(Fout[m2], 3);

			C_MUL(scratch[1], Fout[m], *tw1);
			C_MUL(scratch[2], Fout[m2], *tw2);

			C_ADD(scratch[3], scratch[1], scratch[2]);
			C_SUB(scratch[0], scratch[1], scratch[2]);
			tw1 += fstride;
			tw2 += fstride * 2;

			Fout[m] = cpx_type(Fout->real() - HALF_OF(scratch[3].real()), Fout->imag() - HALF_OF(scratch[3].imag()));

			C_MULBYSCALAR(scratch[0], epi3.imag());

			C_ADDTO(*Fout, scratch[3]);

			Fout[m2] = cpx_type(Fout[m].real() + scratch[0].imag(), Fout[m].imag() - scratch[0].real());

			C_ADDTO(Fout[m], cpx_type(-scratch[0].imag(), scratch[0].real()));
			++Fout;
		} while(--k);
	}

	void kf_bfly5(cpx_type* Fout, const size_t fstride, const size_t m)
	{
		cpx_type* Fout0;
		cpx_type* Fout1;
		cpx_type* Fout2;
		cpx_type* Fout3;
		cpx_type* Fout4;
		size_t u;
		cpx_type scratch[13];
		cpx_type* twiddles = &_twiddles[0];
		cpx_type* tw;
		cpx_type ya, yb;
		ya = twiddles[fstride * m];
		yb = twiddles[fstride * 2 * m];

		Fout0 = Fout;
		Fout1 = Fout0 + m;
		Fout2 = Fout0 + 2 * m;
		Fout3 = Fout0 + 3 * m;
		Fout4 = Fout0 + 4 * m;

		tw = twiddles;
		for(u = 0; u < m; ++u) {
			C_FIXDIV(*Fout0, 5);
			C_FIXDIV(*Fout1, 5);
			C_FIXDIV(*Fout2, 5);
			C_FIXDIV(*Fout3, 5);
			C_FIXDIV(*Fout4, 5);
			scratch[0] = *Fout0;

			C_MUL(scratch[1], *Fout1, tw[u * fstride]);
			C_MUL(scratch[2], *Fout2, tw[2 * u * fstride]);
			C_MUL(scratch[3], *Fout3, tw[3 * u * fstride]);
			C_MUL(scratch[4], *Fout4, tw[4 * u * fstride]);

			C_ADD(scratch[7], scratch[1], scratch[4]);
			C_SUB(scratch[10], scratch[1], scratch[4]);
			C_ADD(scratch[8], scratch[2], scratch[3]);
			C_SUB(scratch[9], scratch[2], scratch[3]);

			C_ADDTO(*Fout0, scratch[7]);
			C_ADDTO(*Fout0, scratch[8]);

			scratch[5] = scratch[0] + cpx_type(S_MUL(scratch[7].real(), ya.real()) + S_MUL(scratch[8].real(), yb.real()), S_MUL(scratch[7].imag(), ya.real())
				+ S_MUL(scratch[8].imag(), yb.real()));

			scratch[6] = cpx_type(S_MUL(scratch[10].imag(), ya.imag()) + S_MUL(scratch[9].imag(), yb.imag()), -S_MUL(scratch[10].real(), ya.imag()) - S_MUL(
				scratch[9].real(), yb.imag()));

			C_SUB(*Fout1, scratch[5], scratch[6]);
			C_ADD(*Fout4, scratch[5], scratch[6]);

			scratch[11] = scratch[0] + cpx_type(S_MUL(scratch[7].real(), yb.real()) + S_MUL(scratch[8].real(), ya.real()), S_MUL(scratch[7].imag(), yb.real())
				+ S_MUL(scratch[8].imag(), ya.real()));

			scratch[12] = cpx_type(-S_MUL(scratch[10].imag(), yb.imag()) + S_MUL(scratch[9].imag(), ya.imag()), S_MUL(scratch[10].real(), yb.imag()) - S_MUL(
				scratch[9].real(), ya.imag()));

			C_ADD(*Fout2, scratch[11], scratch[12]);
			C_SUB(*Fout3, scratch[11], scratch[12]);

			++Fout0;
			++Fout1;
			++Fout2;
			++Fout3;
			++Fout4;
		}
	}

	/* perform the butterfly for one stage of a mixed radix FFT */
	void kf_bfly_generic(cpx_type* Fout, const size_t fstride, int m, int p)
	{
		int u;
		int k;
		int q1;
		int q;
		cpx_type* twiddles = &_twiddles[0];
		cpx_type t;
		int Norig = _nfft;
		cpx_type* scratchbuf = new cpx_type[p];

		for(u = 0; u < m; ++u) {
			k = u;
			for(q1 = 0; q1 < p; ++q1) {
				scratchbuf[q1] = Fout[k];
				C_FIXDIV(scratchbuf[q1], p);
				k += m;
			}

			k = u;
			for(q1 = 0; q1 < p; ++q1) {
				int twidx = 0;
				Fout[k] = scratchbuf[0];
				for(q = 1; q < p; ++q) {
					twidx += fstride * k;
					if(twidx >= Norig)
						twidx -= Norig;
					C_MUL(t, scratchbuf[q], twiddles[twidx]);
					C_ADDTO(Fout[k], t);
				}
				k += m;
			}
		}

		delete[] scratchbuf;
	}

	int _nfft;
	bool _inverse;
	std::vector<cpx_type> _twiddles;
	std::vector<int> _stageRadix;
	std::vector<int> _stageRemainder;
	traits_type _traits;
};
#endif
