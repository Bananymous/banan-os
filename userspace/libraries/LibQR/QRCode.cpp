// written based on https://www.thonky.com/qr-code-tutorial/ and https://tomverbeure.github.io/2022/08/07/Reed-Solomon.html

#include <LibQR/QRCode.h>

#include <BAN/Math.h>
#include <BAN/Vector.h>

namespace LibQR
{

	struct BitStream
	{
		BAN::ErrorOr<void> append(uint32_t value, size_t bits)
		{
			for (size_t i = bits; i > 0; i--)
			{
				if ((length % 8) == 0)
					TRY(data.emplace_back(0));

				data.back() <<= 1;
				if ((value >> (i - 1)) & 1)
					data.back() |= 1;

				length++;
			}

			return {};
		}

		bool operator[](size_t index) const
		{
			ASSERT(index < length);

			const size_t byte = index / 8;
			const size_t bit = index % 8;

			const size_t bits = BAN::Math::min<size_t>(8, length - byte * 8);

			return !!((data[byte] >> (bits - bit - 1)) & 1);
		}

		BAN::Vector<uint8_t> data;
		size_t length { 0 };
	};

	struct GF256
	{
		consteval GF256()
		{
			uint8_t x = 1;

			for (size_t i = 0; i < 256; i++)
			{
				log[x] = i;
				exp[i] = x;

				const uint16_t next = x << 1;
				x = (next < 256) ? next : next ^ 285;
			}

			for (size_t i = 255; i < 512; i++)
				exp[i] = exp[i - 255];

			log[0] = -1;
			log[1] = 0;
		}

		constexpr uint8_t mult(uint8_t a, uint8_t b) const
		{
			if (a == 0 || b == 0)
				return 0;
			return exp[log[a] + log[b]];
		}

		uint8_t exp[512];
		uint8_t log[256];
	};
	static constexpr GF256 s_gf256;

	static BAN::ErrorOr<BAN::Vector<uint8_t>> get_generator(size_t ec_codewords)
	{
		constexpr auto poly_mul =
			[](BAN::ConstByteSpan p, BAN::ConstByteSpan q) -> BAN::ErrorOr<BAN::Vector<uint8_t>>
			{
				BAN::Vector<uint8_t> result;
				TRY(result.resize(p.size() + q.size() - 1, 0));

				for (size_t i = 0; i < p.size(); i++)
				{
					if (p[i] == 0)
						continue;
					for (size_t j = 0; j < q.size(); j++)
						result[i + j] ^= s_gf256.mult(p[i], q[j]);
				}

				return result;
			};

		BAN::Vector<uint8_t> generator;
		TRY(generator.emplace_back(1));

		for (size_t i = 0; i < ec_codewords; i++)
		{
			const uint8_t term[] { s_gf256.exp[i], 1 };
			generator = TRY(poly_mul(generator.span(), BAN::ConstByteSpan::from(term)));
		}

		for (size_t i = 0; i < generator.size() / 2; i++)
			BAN::swap(generator[i], generator[generator.size() - i - 1]);

		return generator;
	}

	static BAN::ErrorOr<BAN::Vector<uint8_t>> get_remainder(BAN::ConstByteSpan message, BAN::ConstByteSpan generator)
	{
		BAN::Vector<uint8_t> dividend;
		TRY(dividend.resize(message.size() + generator.size() - 1, 0));
		for (size_t i = 0; i < message.size(); i++)
			dividend[i] = message[i];

		while (dividend.size() >= generator.size())
		{
			if (const uint8_t scale = dividend[0])
				for (size_t i = 0; i < generator.size(); i++)
					dividend[i] ^= s_gf256.mult(generator[i], scale);
			dividend.remove(0);
		}

		return dividend;
	}

	// s_qr_capacities[x - 1][y] tells the number of bytes version x qr code with error correction y fits
	constexpr size_t s_qr_capacities[][4] {
		{ 17, 14, 11, 7 },
		{ 32, 26, 20, 14 },
		{ 53, 42, 32, 24 },
		{ 78, 62, 46, 34 },
		{ 106, 84, 60, 44 },
		{ 134, 106, 74, 58 },
		{ 154, 122, 86, 64 },
		{ 192, 152, 108, 84 },
		{ 230, 180, 130, 98 },
		{ 271, 213, 151, 119 },
		{ 321, 251, 177, 137 },
		{ 367, 287, 203, 155 },
		{ 425, 331, 241, 177 },
		{ 458, 362, 258, 194 },
		{ 520, 412, 292, 220 },
		{ 586, 450, 322, 250 },
		{ 644, 504, 364, 280 },
		{ 718, 560, 394, 310 },
		{ 792, 624, 442, 338 },
		{ 858, 666, 482, 382 },
		{ 929, 711, 509, 403 },
		{ 1003, 779, 565, 439 },
		{ 1091, 857, 611, 461 },
		{ 1171, 911, 661, 511 },
		{ 1273, 997, 715, 535 },
		{ 1367, 1059, 751, 593 },
		{ 1465, 1125, 805, 625 },
		{ 1528, 1190, 868, 658 },
		{ 1628, 1264, 908, 698 },
		{ 1732, 1370, 982, 742 },
		{ 1840, 1452, 1030, 790 },
		{ 1952, 1538, 1112, 842 },
		{ 2068, 1628, 1168, 898 },
		{ 2188, 1722, 1228, 958 },
		{ 2303, 1809, 1283, 983 },
		{ 2431, 1911, 1351, 1051 },
		{ 2563, 1989, 1423, 1093 },
		{ 2699, 2099, 1499, 1139 },
		{ 2809, 2213, 1579, 1219 },
		{ 2953, 2331, 1663, 1273 },
	};

	/* s_ec_block_info[x - 1][y] describes qr code with version x and error correction y
	elements:
		- ec codewords per block
		- number of blocks in group 1
		- number of data codewords in group 1 blocks
		- number of blocks in group 2
		- number of data codewords in group 2 blocks
	*/
	static constexpr uint8_t s_ec_block_info[][4][5] {
		{ {  7,  1,  19,  0,   0 }, { 10,  1, 16,  0,  0 }, { 13,  1, 13,  0,  0 }, { 17,  1,  9,  0,  0 }, },
		{ { 10,  1,  34,  0,   0 }, { 16,  1, 28,  0,  0 }, { 22,  1, 22,  0,  0 }, { 28,  1, 16,  0,  0 }, },
		{ { 15,  1,  55,  0,   0 }, { 26,  1, 44,  0,  0 }, { 18,  2, 17,  0,  0 }, { 22,  2, 13,  0,  0 }, },
		{ { 20,  1,  80,  0,   0 }, { 18,  2, 32,  0,  0 }, { 26,  2, 24,  0,  0 }, { 16,  4,  9,  0,  0 }, },
		{ { 26,  1, 108,  0,   0 }, { 24,  2, 43,  0,  0 }, { 18,  2, 15,  2, 16 }, { 22,  2, 11,  2, 12 }, },
		{ { 18,  2,  68,  0,   0 }, { 16,  4, 27,  0,  0 }, { 24,  4, 19,  0,  0 }, { 28,  4, 15,  0,  0 }, },
		{ { 20,  2,  78,  0,   0 }, { 18,  4, 31,  0,  0 }, { 18,  2, 14,  4, 15 }, { 26,  4, 13,  1, 14 }, },
		{ { 24,  2,  97,  0,   0 }, { 22,  2, 38,  2, 39 }, { 22,  4, 18,  2, 19 }, { 26,  4, 14,  2, 15 }, },
		{ { 30,  2, 116,  0,   0 }, { 22,  3, 36,  2, 37 }, { 20,  4, 16,  4, 17 }, { 24,  4, 12,  4, 13 }, },
		{ { 18,  2,  68,  2,  69 }, { 26,  4, 43,  1, 44 }, { 24,  6, 19,  2, 20 }, { 28,  6, 15,  2, 16 }, },
		{ { 20,  4,  81,  0,   0 }, { 30,  1, 50,  4, 51 }, { 28,  4, 22,  4, 23 }, { 24,  3, 12,  8, 13 }, },
		{ { 24,  2,  92,  2,  93 }, { 22,  6, 36,  2, 37 }, { 26,  4, 20,  6, 21 }, { 28,  7, 14,  4, 15 }, },
		{ { 26,  4, 107,  0,   0 }, { 22,  8, 37,  1, 38 }, { 24,  8, 20,  4, 21 }, { 22, 12, 11,  4, 12 }, },
		{ { 30,  3, 115,  1, 116 }, { 24,  4, 40,  5, 41 }, { 20, 11, 16,  5, 17 }, { 24, 11, 12,  5, 13 }, },
		{ { 22,  5,  87,  1,  88 }, { 24,  5, 41,  5, 42 }, { 30,  5, 24,  7, 25 }, { 24, 11, 12,  7, 13 }, },
		{ { 24,  5,  98,  1,  99 }, { 28,  7, 45,  3, 46 }, { 24, 15, 19,  2, 20 }, { 30,  3, 15, 13, 16 }, },
		{ { 28,  1, 107,  5, 108 }, { 28, 10, 46,  1, 47 }, { 28,  1, 22, 15, 23 }, { 28,  2, 14, 17, 15 }, },
		{ { 30,  5, 120,  1, 121 }, { 26,  9, 43,  4, 44 }, { 28, 17, 22,  1, 23 }, { 28,  2, 14, 19, 15 }, },
		{ { 28,  3, 113,  4, 114 }, { 26,  3, 44, 11, 45 }, { 26, 17, 21,  4, 22 }, { 26,  9, 13, 16, 14 }, },
		{ { 28,  3, 107,  5, 108 }, { 26,  3, 41, 13, 42 }, { 30, 15, 24,  5, 25 }, { 28, 15, 15, 10, 16 }, },
		{ { 28,  4, 116,  4, 117 }, { 26, 17, 42,  0,  0 }, { 28, 17, 22,  6, 23 }, { 30, 19, 16,  6, 17 }, },
		{ { 28,  2, 111,  7, 112 }, { 28, 17, 46,  0,  0 }, { 30,  7, 24, 16, 25 }, { 24, 34, 13,  0,  0 }, },
		{ { 30,  4, 121,  5, 122 }, { 28,  4, 47, 14, 48 }, { 30, 11, 24, 14, 25 }, { 30, 16, 15, 14, 16 }, },
		{ { 30,  6, 117,  4, 118 }, { 28,  6, 45, 14, 46 }, { 30, 11, 24, 16, 25 }, { 30, 30, 16,  2, 17 }, },
		{ { 26,  8, 106,  4, 107 }, { 28,  8, 47, 13, 48 }, { 30,  7, 24, 22, 25 }, { 30, 22, 15, 13, 16 }, },
		{ { 28, 10, 114,  2, 115 }, { 28, 19, 46,  4, 47 }, { 28, 28, 22,  6, 23 }, { 30, 33, 16,  4, 17 }, },
		{ { 30,  8, 122,  4, 123 }, { 28, 22, 45,  3, 46 }, { 30,  8, 23, 26, 24 }, { 30, 12, 15, 28, 16 }, },
		{ { 30,  3, 117, 10, 118 }, { 28,  3, 45, 23, 46 }, { 30,  4, 24, 31, 25 }, { 30, 11, 15, 31, 16 }, },
		{ { 30,  7, 116,  7, 117 }, { 28, 21, 45,  7, 46 }, { 30,  1, 23, 37, 24 }, { 30, 19, 15, 26, 16 }, },
		{ { 30,  5, 115, 10, 116 }, { 28, 19, 47, 10, 48 }, { 30, 15, 24, 25, 25 }, { 30, 23, 15, 25, 16 }, },
		{ { 30, 13, 115,  3, 116 }, { 28,  2, 46, 29, 47 }, { 30, 42, 24,  1, 25 }, { 30, 23, 15, 28, 16 }, },
		{ { 30, 17, 115,  0,   0 }, { 28, 10, 46, 23, 47 }, { 30, 10, 24, 35, 25 }, { 30, 19, 15, 35, 16 }, },
		{ { 30, 17, 115,  1, 116 }, { 28, 14, 46, 21, 47 }, { 30, 29, 24, 19, 25 }, { 30, 11, 15, 46, 16 }, },
		{ { 30, 13, 115,  6, 116 }, { 28, 14, 46, 23, 47 }, { 30, 44, 24,  7, 25 }, { 30, 59, 16,  1, 17 }, },
		{ { 30, 12, 121,  7, 122 }, { 28, 12, 47, 26, 48 }, { 30, 39, 24, 14, 25 }, { 30, 22, 15, 41, 16 }, },
		{ { 30,  6, 121, 14, 122 }, { 28,  6, 47, 34, 48 }, { 30, 46, 24, 10, 25 }, { 30,  2, 15, 64, 16 }, },
		{ { 30, 17, 122,  4, 123 }, { 28, 29, 46, 14, 47 }, { 30, 49, 24, 10, 25 }, { 30, 24, 15, 46, 16 }, },
		{ { 30,  4, 122, 18, 123 }, { 28, 13, 46, 32, 47 }, { 30, 48, 24, 14, 25 }, { 30, 42, 15, 32, 16 }, },
		{ { 30, 20, 117,  4, 118 }, { 28, 40, 47,  7, 48 }, { 30, 43, 24, 22, 25 }, { 30, 10, 15, 67, 16 }, },
		{ { 30, 19, 118,  6, 119 }, { 28, 18, 47, 31, 48 }, { 30, 34, 24, 34, 25 }, { 30, 20, 15, 61, 16 }, },
	};

	// s_alignment_coords[x - 1] tells the coordinates of alignment patterns in version x qr code
	static constexpr size_t s_alignment_coords[][8] {
		{ 0 },
		{ 6, 18, 0 },
		{ 6, 22, 0 },
		{ 6, 26, 0 },
		{ 6, 30, 0 },
		{ 6, 34, 0 },
		{ 6, 22, 38, 0 },
		{ 6, 24, 42, 0 },
		{ 6, 26, 46, 0 },
		{ 6, 28, 50, 0 },
		{ 6, 30, 54, 0 },
		{ 6, 32, 58, 0 },
		{ 6, 34, 62, 0 },
		{ 6, 26, 46, 66, 0 },
		{ 6, 26, 48, 70, 0 },
		{ 6, 26, 50, 74, 0 },
		{ 6, 30, 54, 78, 0 },
		{ 6, 30, 56, 82, 0 },
		{ 6, 30, 58, 86, 0 },
		{ 6, 34, 62, 90, 0 },
		{ 6, 28, 50, 72,  94, 0 },
		{ 6, 26, 50, 74,  98, 0 },
		{ 6, 30, 54, 78, 102, 0 },
		{ 6, 28, 54, 80, 106, 0 },
		{ 6, 32, 58, 84, 110, 0 },
		{ 6, 30, 58, 86, 114, 0 },
		{ 6, 34, 62, 90, 118, 0 },
		{ 6, 26, 50, 74,  98, 122, 0 },
		{ 6, 30, 54, 78, 102, 126, 0 },
		{ 6, 26, 52, 78, 104, 130, 0 },
		{ 6, 30, 56, 82, 108, 134, 0 },
		{ 6, 34, 60, 86, 112, 138, 0 },
		{ 6, 30, 58, 86, 114, 142, 0 },
		{ 6, 34, 62, 90, 118, 146, 0 },
		{ 6, 30, 54, 78, 102, 126, 150, 0 },
		{ 6, 24, 50, 76, 102, 128, 154, 0 },
		{ 6, 28, 54, 80, 106, 132, 158, 0 },
		{ 6, 32, 58, 84, 110, 136, 162, 0 },
		{ 6, 26, 54, 82, 110, 138, 166, 0 },
		{ 6, 30, 58, 86, 114, 142, 170, 0 },
	};

	enum class ErrorCorrection { L, M, Q, H };

	struct QRInfo
	{
		uint8_t version;
		ErrorCorrection error_correction;
		BitStream bits;
	};

	BAN::ErrorOr<QRCode> QRCode::create(size_t size)
	{
		const size_t bytes = (size * size + 7) / 8;

		uint8_t* data = static_cast<uint8_t*>(BAN::allocator(bytes));
		if (data == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		memset(data, 0, bytes);

		return QRCode(size, data);
	}

	BAN::ErrorOr<QRCode> generate_qr_code(BAN::ConstByteSpan data, ErrorCorrection ec);

	BAN::ErrorOr<QRCode> QRCode::generate(BAN::ConstByteSpan data)
	{
		return generate_qr_code(data, ErrorCorrection::L);
	}

	BAN::ErrorOr<QRCode> QRCode::copy() const
	{
		const size_t bytes = (m_size * m_size + 7) / 8;

		uint8_t* data = static_cast<uint8_t*>(BAN::allocator(bytes));
		if (data == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		memcpy(data, m_data, bytes);

		return QRCode(m_size, data);
	}

	QRCode::QRCode(size_t size, uint8_t* data)
		: m_size(size)
		, m_data(data)
	{ }

	QRCode::QRCode(QRCode&& other)
		: m_size(other.m_size)
		, m_data(other.m_data)
	{
		other.m_data = nullptr;
	}

	QRCode::~QRCode()
	{
		if (m_data != nullptr)
			BAN::deallocator(m_data);
		m_data = nullptr;
	}

	void QRCode::set(size_t x, size_t y, bool value)
	{
		ASSERT(x < m_size && y < m_size);

		const size_t index = y * m_size + x;
		const size_t byte = index / 8;
		const size_t bit = index % 8;

		if (value)
			m_data[byte] |= 1 << bit;
		else
			m_data[byte] &= ~(1 << bit);
	}

	void QRCode::toggle(size_t x, size_t y)
	{
		ASSERT(x < m_size && y < m_size);

		const size_t index = y * m_size + x;
		const size_t byte = index / 8;
		const size_t bit = index % 8;

		m_data[byte] ^= 1 << bit;
	}

	bool QRCode::get(size_t x, size_t y) const
	{
		ASSERT(x < m_size && y < m_size);

		const size_t index = y * m_size + x;
		const size_t byte = index / 8;
		const size_t bit = index % 8;

		return (m_data[byte] >> bit) & 1;
	}

	size_t QRCode::size() const
	{
		return m_size;
	}

	static BAN::ErrorOr<QRInfo> generate_data(BAN::ConstByteSpan data, ErrorCorrection error_correction)
	{
		QRInfo qr_info;
		qr_info.error_correction = error_correction;

		qr_info.version = 0xFF;
		for (size_t i = 0; i < sizeof(s_qr_capacities) / sizeof(s_qr_capacities[0]); i++)
		{
			if (data.size() > s_qr_capacities[i][static_cast<size_t>(error_correction)])
				continue;
			qr_info.version = i + 1;
			break;
		}
		if (qr_info.version == 0xFF)
			return BAN::Error::from_errno(E2BIG);

		// byte mode
		TRY(qr_info.bits.append(0b0100, 4));

		// data length
		TRY(qr_info.bits.append(data.size(), (qr_info.version <= 9) ? 8 : 16));

		// data
		for (size_t i = 0; i < data.size(); i++)
			TRY(qr_info.bits.append(data[i], 8));

		auto ec_info = s_ec_block_info[qr_info.version - 1][static_cast<size_t>(qr_info.error_correction)];
		const size_t max_bits = (ec_info[1] * ec_info[2] + ec_info[3] * ec_info[4]) * 8;
		ASSERT(qr_info.bits.length <= max_bits);

		// terminator
		if (const size_t missing = max_bits - qr_info.bits.length; missing < 4)
			TRY(qr_info.bits.append(0, missing));
		else
			TRY(qr_info.bits.append(0, 4));

		// byte align
		if (const size_t rem = qr_info.bits.length % 8)
			TRY(qr_info.bits.append(0, 8 - rem));

		// add pad bytes
		for (bool toggle = true; qr_info.bits.length < max_bits; toggle = !toggle)
			TRY(qr_info.bits.append(toggle ? 0b11101100 : 0b00010001, 8));

		BAN::ConstByteSpan data_words = qr_info.bits.data.span();

		// break into data blocks for error correction
		BAN::Vector<BAN::ConstByteSpan> data_blocks;
		for (size_t group = 0; group < 2; group++)
		{
			const size_t nblock = ec_info[group * 2 + 1];
			const size_t nwords = ec_info[group * 2 + 2];

			for (size_t i = 0; i < nblock; i++)
			{
				TRY(data_blocks.push_back(data_words.slice(0, nwords)));
				data_words = data_words.slice(nwords);
			}
		}

		ASSERT(data_words.empty());

		// calculate error blocks
		const auto generator = TRY(get_generator(ec_info[0]));
		BAN::Vector<BAN::Vector<uint8_t>> ec_blocks;
		for (const auto& data_block : data_blocks)
			TRY(ec_blocks.push_back(TRY(get_remainder(data_block, generator.span()))));

		// interleave data and error blocks
		BAN::Vector<uint8_t> interleaved;
		TRY(interleaved.reserve(
			ec_info[1] * (ec_info[2] + ec_info[0]) +
			ec_info[3] * (ec_info[4] + ec_info[0])
		));
		for (size_t i = 0; i < ec_info[2]; i++)
			for (size_t j = 0; j < ec_info[1] + ec_info[3]; j++)
				MUST(interleaved.push_back(data_blocks[j][i]));
		for (size_t j = 0; j < ec_info[3]; j++)
			MUST(interleaved.push_back(data_blocks[ec_info[1] + j][ec_info[2]]));
		for (size_t i = 0; i < ec_info[0]; i++)
			for (size_t j = 0; j < ec_blocks.size(); j++)
				MUST(interleaved.push_back(ec_blocks[j][i]));

		// update returned info and append required remainder bits
		qr_info.bits.data = BAN::move(interleaved);
		qr_info.bits.length = qr_info.bits.data.size() * 8;

		constexpr uint8_t remainer_bits[] {
			0, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0,
		};
		TRY(qr_info.bits.append(0, remainer_bits[qr_info.version - 1]));

		return qr_info;
	}

	struct PrepareMatrixResult
	{
		QRCode qr_code;
		QRCode reserved;
	};

	static BAN::ErrorOr<PrepareMatrixResult> prepare_matrix(uint8_t version)
	{
		const size_t size = (version - 1) * 4 + 21;

		auto qr_code = TRY(QRCode::create(size));
		auto reserved = TRY(QRCode::create(size));

		// finder patterns
		{
			const auto place_finder =
				[&qr_code, &reserved](size_t x, size_t y)
				{
					for (size_t i = 0; i < 7; i++)
					{
						qr_code.set(x,     y + i, true);
						qr_code.set(x + i, y,     true);
						qr_code.set(x + 6, y + i, true);
						qr_code.set(x + i, y + 6, true);
					}

					for (size_t i = 0; i < 3; i++)
						for (size_t j = 0; j < 3; j++)
							qr_code.set(x + j + 2, y + i + 2, true);

					if (x) x--;
					if (y) y--;
					for (size_t i = 0; i < 8; i++)
						for (size_t j = 0; j < 8; j++)
							reserved.set(x + j, y + i, true);
				};

			place_finder(0, 0);
			place_finder(0, size - 7);
			place_finder(size - 7, 0);
		}

		// alignment patterns
		{
			const auto place_alignment =
				[&qr_code, &reserved](size_t x, size_t y)
				{
					for (ssize_t i = -2; i <= 2; i++)
						for (ssize_t j = -2; j <= 2; j++)
							if (reserved.get(x + j, y + i))
								return;

					qr_code.set(x, y, true);
					for (ssize_t i = -2; i <= 2; i++)
					{
						qr_code.set(x + i, y - 2, true);
						qr_code.set(x + i, y + 2, true);
						qr_code.set(x - 2, y + i, true);
						qr_code.set(x + 2, y + i, true);
					}

					for (ssize_t i = -2; i <= 2; i++)
						for (ssize_t j = -2; j <= 2; j++)
							reserved.set(x + j, y + i, true);
				};

			const auto& coords = s_alignment_coords[version - 1];
			for (size_t i = 0; coords[i]; i++)
				for (size_t j = 0; coords[j]; j++)
					place_alignment(coords[i], coords[j]);
		}

		// timing patterns
		{
			bool toggle = true;
			for (size_t i = 8; i < size - 8; i++)
			{
				qr_code.set(i, 6, toggle);
				qr_code.set(6, i, toggle);
				toggle = !toggle;

				reserved.set(i, 6, true);
				reserved.set(6, i, true);
			}
		}

		// dark module and format information area
		{
			qr_code.set(8, size - 8, true);

			reserved.set(8, 8, true);
			for (size_t i = 0; i < 8; i++)
			{
				reserved.set(i, 8, true);
				reserved.set(8, i, true);
				reserved.set(8, size - 8 + i, true);
				reserved.set(size - 8 + i, 8, true);
			}
		}

		// version information area
		if (version >= 7)
		{
			for (size_t i = 0; i < 6; i++)
			{
				for (size_t j = 0; j < 3; j++)
				{
					reserved.set(i, size - 11 + j, true);
					reserved.set(size - 11 + j, i, true);
				}
			}
		}

		return PrepareMatrixResult { BAN::move(qr_code), BAN::move(reserved) };
	}

	static size_t evaluate_qr_code(const QRCode& qr_code)
	{
		const size_t size = qr_code.size();

		size_t score = 0;

		// condition 1
		{
			for (size_t y = 0; y < size; y++)
			{
				for (size_t x = 0; x < size;)
				{
					size_t consecutive = 1;
					while (x + consecutive + 1 < size && qr_code.get(x, y) == qr_code.get(x + consecutive + 1, y))
						consecutive++;
					if (consecutive >= 5)
						score += consecutive - 2;
					x += consecutive;
				}
			}

			for (size_t x = 0; x < size; x++)
			{
				for (size_t y = 0; y < size;)
				{
					size_t consecutive = 1;
					while (y + consecutive + 1 < size && qr_code.get(x, y) == qr_code.get(x, y + consecutive + 1))
						consecutive++;
					if (consecutive >= 5)
						score += consecutive - 2;
					y += consecutive;
				}
			}
		}

		// condition 2
		{
			for (size_t y = 0; y < size - 1; y++)
			{
				for (size_t x = 0; x < size - 1; x++)
				{
					if (qr_code.get(x, y) != qr_code.get(x + 1, y))
						continue;
					if (qr_code.get(x, y) != qr_code.get(x, y + 1))
						continue;
					if (qr_code.get(x, y) != qr_code.get(x + 1, y + 1))
						continue;
					score += 3;
				}
			}
		}

		// condition 3
		{
			const bool targets[][11] {
				{ 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1 },
			};

			for (size_t y = 0; y < size; y++)
			{
				for (size_t x = 0; x < size - 11; x++)
				{
					for (auto& target : targets)
					{
						bool match = true;
						for (size_t i = 0; i < 11 && match; i++)
							if (qr_code.get(x + 1, y) != target[i])
								match = false;
						if (match)
							score += 40;
					}
				}
			}

			for (size_t x = 0; x < size; x++)
			{
				for (size_t y = 0; y < size - 11; y++)
				{
					for (auto& target : targets)
					{
						bool match = true;
						for (size_t i = 0; i < 11 && match; i++)
							if (qr_code.get(x, y + i) != target[i])
								match = false;
						if (match)
							score += 40;
					}
				}
			}
		}

		// condition 4
		{
			size_t dark_modules = 0;
			for (size_t y = 0; y < qr_code.size(); y++)
				for (size_t x = 0; x < qr_code.size(); x++)
					dark_modules += qr_code.get(x, y);

			const size_t ratio = 100 * dark_modules / (size * size * 5);

			const size_t temp1 = BAN::Math::max<size_t>(ratio,     10) - BAN::Math::min<size_t>(ratio,     10);
			const size_t temp2 = BAN::Math::max<size_t>(ratio + 1, 10) - BAN::Math::min<size_t>(ratio + 1, 10);
			score += BAN::Math::min<size_t>(temp1, temp2) * 10;
		}

		return score;
	}

	static BAN::ErrorOr<uint8_t> apply_mask_pattern(QRCode& qr_code, const QRCode& reserved)
	{
		const size_t size = qr_code.size();

		bool (*mask_pattern_funcs[])(size_t, size_t) {
			[](size_t c, size_t r) { return (r + c) % 2 == 0; },
			[](size_t  , size_t r) { return r % 2 == 0; },
			[](size_t c, size_t  ) { return c % 3 == 0; },
			[](size_t c, size_t r) { return (r + c) % 3 == 0; },
			[](size_t c, size_t r) { return (r / 2 + c / 3) % 2 == 0; },
			[](size_t c, size_t r) { return (r * c) % 2 + (r * c) % 3 == 0; },
			[](size_t c, size_t r) { return ((r * c) % 3 + r * c) % 2 == 0; },
			[](size_t c, size_t r) { return ((r * c) % 3 + r + c) % 2 == 0; },
		};

		size_t best_pattern = 0;
		size_t best_score = SIZE_MAX;

		for (size_t i = 0; i < sizeof(mask_pattern_funcs) / sizeof(*mask_pattern_funcs); i++)
		{
			auto temp = TRY(qr_code.copy());

			for (size_t y = 0; y < size; y++)
				for (size_t x = 0; x < size; x++)
					if (!reserved.get(x, y) && mask_pattern_funcs[i](x, y))
						temp.toggle(x, y);

			if (const size_t score = evaluate_qr_code(temp); score < best_score)
			{
				best_pattern = i;
				best_score = score;
			}
		}

		for (size_t y = 0; y < size; y++)
			for (size_t x = 0; x < size; x++)
				if (!reserved.get(x, y) && mask_pattern_funcs[best_pattern](x, y))
					qr_code.toggle(x, y);

		return best_pattern;
	}

	BAN::ErrorOr<QRCode> generate_qr_code(BAN::ConstByteSpan data, ErrorCorrection ec)
	{
		const auto qr_info = TRY(generate_data({ reinterpret_cast<const uint8_t*>(data.data()), data.size() }, ec));

		auto [qr_code, reserved] = TRY(prepare_matrix(qr_info.version));
		const size_t size = qr_code.size();

		{
			size_t index = 0;

			bool toggle = true;

			size_t x = size;
			while (x > 0)
			{
				x -= 2;
				if (x == 5)
					x--;

				const ssize_t y_s = toggle ? size - 1 : 0;
				const ssize_t y_e = toggle ? -1 : size;
				const ssize_t dir = toggle ? -1 : 1;
				toggle = !toggle;

				for (ssize_t y = y_s; y != y_e; y += dir)
				{
					if (!reserved.get(x + 1, y))
						qr_code.set(x + 1, y, qr_info.bits[index++]);
					if (!reserved.get(x + 0, y))
						qr_code.set(x + 0, y, qr_info.bits[index++]);
				}
			}

			ASSERT(index == qr_info.bits.length);
		}

		const auto mod2_remainder =
			[](uint32_t data, uint32_t generator, uint32_t degree)
			{
				constexpr auto bits = [](uint32_t val) -> uint32_t { return 31 - __builtin_clz(val | 1); };
				while (bits(data) >= degree)
					data ^= generator << (bits(data) - degree);
				return data;
			};

		// format string
		{
			const uint8_t pattern = TRY(apply_mask_pattern(qr_code, reserved));
			const uint8_t ec_to_val[] { 1, 0, 3, 2 };

			const uint16_t format_data = (ec_to_val[static_cast<size_t>(qr_info.error_correction)] << 13) | (pattern << 10);
			const uint16_t format_string = (format_data | mod2_remainder(format_data, 0b10100110111, 10)) ^ 0b101010000010010;

			constexpr int8_t format_location[][2][2] {
				{ { 8, 0 }, { -1,  8 } },
				{ { 8, 1 }, { -2,  8 } },
				{ { 8, 2 }, { -3,  8 } },
				{ { 8, 3 }, { -4,  8 } },
				{ { 8, 4 }, { -5,  8 } },
				{ { 8, 5 }, { -6,  8 } },
				{ { 8, 7 }, { -7,  8 } },
				{ { 8, 8 }, { -8,  8 } },
				{ { 7, 8 }, {  8, -7 } },
				{ { 5, 8 }, {  8, -6 } },
				{ { 4, 8 }, {  8, -5 } },
				{ { 3, 8 }, {  8, -4 } },
				{ { 2, 8 }, {  8, -3 } },
				{ { 1, 8 }, {  8, -2 } },
				{ { 0, 8 }, {  8, -1 } },
			};

			for (size_t bit = 0; bit < 15; bit++)
			{
				for (size_t i = 0; i < 2; i++)
				{
					const auto [x, y] = format_location[bit][i];
					qr_code.set(
						x < 0 ? size + x : x,
						y < 0 ? size + y : y,
						(format_string >> bit) & 1
					);
				}
			}
		}

		// version string
		if (qr_info.version >= 7)
		{
			const uint32_t version_data = qr_info.version << 12;
			const uint32_t version_string = (version_data | mod2_remainder(version_data, 0b1111100100101, 12));

			for (size_t i = 0; i < 6; i++)
			{
				for (size_t j = 0; j < 3; j++)
				{
					qr_code.set(i, size - 11 + j, (version_string >> (i * 3 + j)) & 1);
					qr_code.set(size - 11 + j, i, (version_string >> (i * 3 + j)) & 1);
				}
			}
		}

		return BAN::move(qr_code);
	}

}
