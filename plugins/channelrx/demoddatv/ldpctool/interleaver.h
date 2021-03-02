/*
Bit interleaver

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef INTERLEAVER_HH
#define INTERLEAVER_HH

namespace ldpctool {

template <typename TYPE>
struct Interleaver
{
	virtual void fwd(TYPE *) = 0;
	virtual void bwd(TYPE *) = 0;
	virtual ~Interleaver() = default;
};

template <typename TYPE>
struct ITL0 : public Interleaver<TYPE>
{
	void fwd(TYPE *){}
	void bwd(TYPE *){}
};

template <typename TYPE, typename PITL, typename MUX>
struct BITL : public Interleaver<TYPE>
{
	static const int N = PITL::N;
	static const int COLS = MUX::N;
	static const int ROWS = N / COLS;
	TYPE tmp[N];
	void fwd(TYPE *io)
	{
		PITL::fwd(tmp, io);
		for (int row = 0; row < ROWS; ++row)
			MUX::fwd(io+COLS*row, tmp+row, ROWS);
	}
	void bwd(TYPE *io)
	{
		for (int row = 0; row < ROWS; ++row)
			MUX::bwd(tmp+row, io+COLS*row, ROWS);
		PITL::bwd(io, tmp);
	}
};

template <typename TYPE, int NUM>
struct PITL0
{
	static const int N = NUM;
	static void fwd(TYPE *out, TYPE *in)
	{
		for (int n = 0; n < N; ++n)
			out[n] = in[n];
	}
	static void bwd(TYPE *out, TYPE *in)
	{
		for (int n = 0; n < N; ++n)
			out[n] = in[n];
	}
};

template <typename TYPE, int NUM, int Q>
struct PITL
{
	static const int N = NUM;
	static const int M = 360;
	static const int K = N - M * Q;
	static void fwd(TYPE *out, TYPE *in)
	{
		for (int k = 0; k < K; ++k)
			out[k] = in[k];
		for (int q = 0; q < Q; ++q)
			for (int m = 0; m < M; ++m)
				out[K+M*q+m] = in[K+Q*m+q];
	}
	static void bwd(TYPE *out, TYPE *in)
	{
		for (int k = 0; k < K; ++k)
			out[k] = in[k];
		for (int q = 0; q < Q; ++q)
			for (int m = 0; m < M; ++m)
				out[K+Q*m+q] = in[K+M*q+m];
	}
};

template <typename TYPE, int NUM, int Q, typename CT>
struct PCTITL
{
	static const int N = NUM;
	static const int COLS = CT::N;
	static const int ROWS = N / COLS;
	static void fwd(TYPE *out, TYPE *in)
	{
		PITL<TYPE, N, Q>::fwd(out, in);
		for (int n = 0; n < N; ++n)
			in[n] = out[n];
		for (int row = 0; row < ROWS; ++row)
			CT::fwd(out+COLS*row, in, ROWS, row);
	}
	static void bwd(TYPE *out, TYPE *in)
	{
		for (int row = 0; row < ROWS; ++row)
			CT::bwd(out, in+COLS*row, ROWS, row);
		for (int n = 0; n < N; ++n)
			in[n] = out[n];
		PITL<TYPE, N, Q>::bwd(out, in);
	}
};

template <typename TYPE>
struct MUX0
{
	static const int N = 1;
	static void fwd(TYPE *out, TYPE *in, int)
	{
		out[0] = in[0];
	}
	static void bwd(TYPE *out, TYPE *in, int)
	{
		out[0] = in[0];
	}
};

template <typename TYPE, int E0, int E1, int E2>
struct MUX3
{
	static const int N = 3;
	static void fwd(TYPE *out, TYPE *in, int S)
	{
		out[E0] = in[0*S];
		out[E1] = in[1*S];
		out[E2] = in[2*S];
	}
	static void bwd(TYPE *out, TYPE *in, int S)
	{
		out[0*S] = in[E0];
		out[1*S] = in[E1];
		out[2*S] = in[E2];
	}
};

template <typename TYPE, int E0, int E1, int E2, int E3, int E4, int E5, int E6, int E7>
struct MUX8
{
	static const int N = 8;
	static void fwd(TYPE *out, TYPE *in, int S)
	{
		out[E0] = in[0*S];
		out[E1] = in[1*S];
		out[E2] = in[2*S];
		out[E3] = in[3*S];
		out[E4] = in[4*S];
		out[E5] = in[5*S];
		out[E6] = in[6*S];
		out[E7] = in[7*S];
	}
	static void bwd(TYPE *out, TYPE *in, int S)
	{
		out[0*S] = in[E0];
		out[1*S] = in[E1];
		out[2*S] = in[E2];
		out[3*S] = in[E3];
		out[4*S] = in[E4];
		out[5*S] = in[E5];
		out[6*S] = in[E6];
		out[7*S] = in[E7];
	}
};

template <typename TYPE, int E0, int E1, int E2, int E3, int E4, int E5, int E6, int E7, int E8, int E9, int E10, int E11>
struct MUX12
{
	static const int N = 12;
	static void fwd(TYPE *out, TYPE *in, int S)
	{
		out[E0] = in[0*S];
		out[E1] = in[1*S];
		out[E2] = in[2*S];
		out[E3] = in[3*S];
		out[E4] = in[4*S];
		out[E5] = in[5*S];
		out[E6] = in[6*S];
		out[E7] = in[7*S];
		out[E8] = in[8*S];
		out[E9] = in[9*S];
		out[E10] = in[10*S];
		out[E11] = in[11*S];
	}
	static void bwd(TYPE *out, TYPE *in, int S)
	{
		out[0*S] = in[E0];
		out[1*S] = in[E1];
		out[2*S] = in[E2];
		out[3*S] = in[E3];
		out[4*S] = in[E4];
		out[5*S] = in[E5];
		out[6*S] = in[E6];
		out[7*S] = in[E7];
		out[8*S] = in[E8];
		out[9*S] = in[E9];
		out[10*S] = in[E10];
		out[11*S] = in[E11];
	}
};

template <typename TYPE, int E0, int E1, int E2, int E3, int E4, int E5, int E6, int E7, int E8, int E9, int E10, int E11, int E12, int E13, int E14, int E15>
struct MUX16
{
	static const int N = 16;
	static void fwd(TYPE *out, TYPE *in, int S)
	{
		out[E0] = in[0*S];
		out[E1] = in[1*S];
		out[E2] = in[2*S];
		out[E3] = in[3*S];
		out[E4] = in[4*S];
		out[E5] = in[5*S];
		out[E6] = in[6*S];
		out[E7] = in[7*S];
		out[E8] = in[8*S];
		out[E9] = in[9*S];
		out[E10] = in[10*S];
		out[E11] = in[11*S];
		out[E12] = in[12*S];
		out[E13] = in[13*S];
		out[E14] = in[14*S];
		out[E15] = in[15*S];
	}
	static void bwd(TYPE *out, TYPE *in, int S)
	{
		out[0*S] = in[E0];
		out[1*S] = in[E1];
		out[2*S] = in[E2];
		out[3*S] = in[E3];
		out[4*S] = in[E4];
		out[5*S] = in[E5];
		out[6*S] = in[E6];
		out[7*S] = in[E7];
		out[8*S] = in[E8];
		out[9*S] = in[E9];
		out[10*S] = in[E10];
		out[11*S] = in[E11];
		out[12*S] = in[E12];
		out[13*S] = in[E13];
		out[14*S] = in[E14];
		out[15*S] = in[E15];
	}
};

template <typename TYPE, int T0, int T1, int T2, int T3, int T4, int T5, int T6, int T7>
struct CT8
{
	static const int N = 8;
	static void fwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0] = in[0*S+(R+S-T0)%S];
		out[1] = in[1*S+(R+S-T1)%S];
		out[2] = in[2*S+(R+S-T2)%S];
		out[3] = in[3*S+(R+S-T3)%S];
		out[4] = in[4*S+(R+S-T4)%S];
		out[5] = in[5*S+(R+S-T5)%S];
		out[6] = in[6*S+(R+S-T6)%S];
		out[7] = in[7*S+(R+S-T7)%S];
	}
	static void bwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0*S+(R+S-T0)%S] = in[0];
		out[1*S+(R+S-T1)%S] = in[1];
		out[2*S+(R+S-T2)%S] = in[2];
		out[3*S+(R+S-T3)%S] = in[3];
		out[4*S+(R+S-T4)%S] = in[4];
		out[5*S+(R+S-T5)%S] = in[5];
		out[6*S+(R+S-T6)%S] = in[6];
		out[7*S+(R+S-T7)%S] = in[7];
	}
};

template <typename TYPE, int T0, int T1, int T2, int T3, int T4, int T5, int T6, int T7, int T8, int T9, int T10, int T11>
struct CT12
{
	static const int N = 12;
	static void fwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0] = in[0*S+(R+S-T0)%S];
		out[1] = in[1*S+(R+S-T1)%S];
		out[2] = in[2*S+(R+S-T2)%S];
		out[3] = in[3*S+(R+S-T3)%S];
		out[4] = in[4*S+(R+S-T4)%S];
		out[5] = in[5*S+(R+S-T5)%S];
		out[6] = in[6*S+(R+S-T6)%S];
		out[7] = in[7*S+(R+S-T7)%S];
		out[8] = in[8*S+(R+S-T8)%S];
		out[9] = in[9*S+(R+S-T9)%S];
		out[10] = in[10*S+(R+S-T10)%S];
		out[11] = in[11*S+(R+S-T11)%S];
	}
	static void bwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0*S+(R+S-T0)%S] = in[0];
		out[1*S+(R+S-T1)%S] = in[1];
		out[2*S+(R+S-T2)%S] = in[2];
		out[3*S+(R+S-T3)%S] = in[3];
		out[4*S+(R+S-T4)%S] = in[4];
		out[5*S+(R+S-T5)%S] = in[5];
		out[6*S+(R+S-T6)%S] = in[6];
		out[7*S+(R+S-T7)%S] = in[7];
		out[8*S+(R+S-T8)%S] = in[8];
		out[9*S+(R+S-T9)%S] = in[9];
		out[10*S+(R+S-T10)%S] = in[10];
		out[11*S+(R+S-T11)%S] = in[11];
	}
};

template <typename TYPE, int T0, int T1, int T2, int T3, int T4, int T5, int T6, int T7, int T8, int T9, int T10, int T11, int T12, int T13, int T14, int T15>
struct CT16
{
	static const int N = 16;
	static void fwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0] = in[0*S+(R+S-T0)%S];
		out[1] = in[1*S+(R+S-T1)%S];
		out[2] = in[2*S+(R+S-T2)%S];
		out[3] = in[3*S+(R+S-T3)%S];
		out[4] = in[4*S+(R+S-T4)%S];
		out[5] = in[5*S+(R+S-T5)%S];
		out[6] = in[6*S+(R+S-T6)%S];
		out[7] = in[7*S+(R+S-T7)%S];
		out[8] = in[8*S+(R+S-T8)%S];
		out[9] = in[9*S+(R+S-T9)%S];
		out[10] = in[10*S+(R+S-T10)%S];
		out[11] = in[11*S+(R+S-T11)%S];
		out[12] = in[12*S+(R+S-T12)%S];
		out[13] = in[13*S+(R+S-T13)%S];
		out[14] = in[14*S+(R+S-T14)%S];
		out[15] = in[15*S+(R+S-T15)%S];
	}
	static void bwd(TYPE *out, TYPE *in, int S, int R)
	{
		out[0*S+(R+S-T0)%S] = in[0];
		out[1*S+(R+S-T1)%S] = in[1];
		out[2*S+(R+S-T2)%S] = in[2];
		out[3*S+(R+S-T3)%S] = in[3];
		out[4*S+(R+S-T4)%S] = in[4];
		out[5*S+(R+S-T5)%S] = in[5];
		out[6*S+(R+S-T6)%S] = in[6];
		out[7*S+(R+S-T7)%S] = in[7];
		out[8*S+(R+S-T8)%S] = in[8];
		out[9*S+(R+S-T9)%S] = in[9];
		out[10*S+(R+S-T10)%S] = in[10];
		out[11*S+(R+S-T11)%S] = in[11];
		out[12*S+(R+S-T12)%S] = in[12];
		out[13*S+(R+S-T13)%S] = in[13];
		out[14*S+(R+S-T14)%S] = in[14];
		out[15*S+(R+S-T15)%S] = in[15];
	}
};

} // namespace ldpctool

#endif

