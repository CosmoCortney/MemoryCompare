#pragma once
#include "MemoryDump.hpp"
#include <functional>
#include <iostream>
#include "MemoryCompareResult.hpp"
#include "MemoryCompareOperators.hpp"
#include <type_traits>
#include "LitColor.hpp"
#include "MorphText.hpp"
#include "OperativeArray.hpp"
#include <fstream>

namespace MemoryCompare
{
	enum Conditions
	{
		EQUAL = 0,
		UNEQUAL = 1,
		GREATER = 2,
		GREATER_EQUAL = 3,
		LOWER = 4,
		LOWER_EQUAL = 5,
		INCREASED_BY = 6,
		DECREASED_BY = 7,
		BETWEEN = 8,
		NOT_BETWEEN = 9,
		AND = 10,
		OR = 11,
	};

	enum ValueType
	{
		PRIMITIVE = 0,
		ARRAY = 1,
		TEXT = 2,
		COLOR = 3
	};

	enum PrimitiveType
	{
		INT8 = 0,
		INT16 = 1,
		INT32 = 2,
		INT64 = 3,
		FLOAT = 4,
		DOUBLE = 5,
		BOOL = 6 //todo
	};

	enum SetupFlags
	{
		SIGNED = 1,
		CASE_SENSITIVE = 1 << 1,
		BIG_ENDIAN = 1 << 2,
		REWIND_ENABLED = 1 << 3,
		CACHED = 1 << 4,
		ZIP_RESULTS = 1 << 5,
		DISABLE_UNDO = 1 << 6
	};

	enum IterationFlags
	{
		KNOWN = 1,
		HEX = 1 << 1
	};

	template<typename T> static inline T SwapBytes(T val)
	{
		uint8_t* temp = (uint8_t*)&val;
		struct LoopIndices { uint8_t first; uint8_t last; };
		LoopIndices i = { 0, sizeof(val) - 1 };

		for (; i.first < sizeof(val) >> 1; ++i.first, --i.last)
		{
			temp[i.first] ^= temp[i.last];
			temp[i.last] ^= temp[i.first];
			temp[i.first] ^= temp[i.last];
		}

		return val;
	}

	template <typename T, template<typename...> class Template>
	struct is_instantiation_of : std::false_type {};

	template <template<typename...> class Template, typename... Args>
	struct is_instantiation_of<Template<Args...>, Template> : std::true_type {};

	static bool SaveBinary(const std::wstring& filePath, void* data, const uint64_t size, const bool append/*, const bool zip*/)
	{
		std::ofstream file(filePath, std::ios::binary | (append ? std::ios_base::app : std::ios::out));
		if (!file)
			return false;

		/*if (zip)
		{
			const int CHUNK_SIZE = 0x4000;
			char out_buffer[CHUNK_SIZE];

			z_stream strm;
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			deflateInit(&strm, Z_DEFAULT_COMPRESSION);

			strm.avail_in = size;
			strm.next_in = reinterpret_cast<Bytef*>((char*)data);

			int bytes_written;
			do {
				strm.avail_out = CHUNK_SIZE;
				strm.next_out = reinterpret_cast<Bytef*>(out_buffer);
				deflate(&strm, Z_FINISH);
				bytes_written = CHUNK_SIZE - strm.avail_out;
				file.write(out_buffer, bytes_written);
			} while (strm.avail_out == 0);

			deflateEnd(&strm);
			file.close();

			return true;
		}*/

		file.write((char*)data, size);
		file.close();

		return true;
	}

	static bool LoadBinary(const std::wstring& filePath, void* out, uint64_t& size, const uint64_t startPos)
	{
		std::ifstream file;
		file.open(filePath, std::ios::binary | std::ios::in);

		if (file.is_open())
		{
			file.seekg(0, std::ios::end);
			uint64_t end = file.tellg();
			file.seekg(0, std::ios::beg);

			if (size == 0 && startPos == 0) //read all
			{
				size = end;
			}
			else //read fraction
			{
				uint64_t readableSize = end - startPos;
				file.seekg(startPos, std::ios::cur);

				if (readableSize < size || size == 0)
				{
					size = readableSize;
				}
			}

			if (!file.read((char*)out, size))
			{
				return false;
			}
		}
		file.close();
		return true;
	}

	class MemCompare
	{
	private:
		MemCompare(){}
		MemCompare(MemCompare const&) = delete;
		void operator=(MemCompare const&) = delete;
		static MemCompare& GetInstance()
		{
			static MemCompare Instance;
			return Instance;
		}

		//Results
		static std::vector<MemCompareResults> _results;
		uint32_t _iterationCount = 0;
		uint64_t _resultCount = 0;

		//Main SetUp
		uint32_t _setupFlags = 0;
		std::wstring _resultsDir;
		uint16_t _superiorDatatype = 0;
		uint16_t _subsidiaryDatatype = 2;
		uint8_t _addressWidth = 4;
		uint8_t _valueWidth = 4;
		uint16_t _alignment = 4;
		std::string _primaryKnownValueStr;
		std::string _secondaryKnownValueStr;
		
		//Iteration
		uint32_t _iterationFlags = 0;
		uint8_t _condition = 0;
		uint16_t _counterIteration = 0;
		uint16_t _counterIterationIndex = 0;
		float _precision = 1.0f;

		//current range
		uint16_t _rangeCount = 0;
		int _previousIterationRangeIndex = -1;
		char* _currentDumpAddress = nullptr;
		uint64_t _currentDumpSize = 0;
		uint64_t _currentBaseAddress = 0;

		void selectPrimitiveUnknownInitial();
		void selectPrimitiveUnknownSuccessive();
		void selectPrimitiveKnownInitial();
		void selectPrimitiveKnownSuccessive();
		void selectColorKnownInitial();
		void selectColorKnownSuccessive();
		void selectTextKnownInitial();
		void selectArrayKnownInitial();
		void selectArrayKnownSuccessive(); //ToDo, signed values, floats
		void setValueWidth();
		bool saveResults();

		template<typename dataType> dataType parseKnownValue(std::string& knownValue, const bool hex)
		{
			std::stringstream stream;
			dataType val;

			if constexpr (std::is_integral_v<dataType>)
			{
				if (hex)
					stream << std::hex << knownValue;
				else
					stream << knownValue;

				if constexpr (std::is_same_v<dataType, uint8_t> || std::is_same_v<dataType, int8_t>)
				{
					static uint16_t temp;
					stream >> temp;
					val = temp;
				}
				else
					stream >> val;
			}
			else if constexpr (std::is_floating_point_v<dataType>)
			{
				stream << knownValue;
				stream >> val;
			}

			return val;
		}

		template<typename dataType> void setUpComparasionOperator(CompareOperator<dataType>& comparisonOperator)
		{
			switch (_condition)
			{
			case EQUAL:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::equal_precision;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::equal_color;
				else// if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::equal;
				break;
			case UNEQUAL:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::not_equal_precision;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::not_equal_color;
				else// if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::not_equal;
				break;
			case GREATER:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::greater_precision;
				else if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::greater;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::greater_color;
				else //OperativeArray
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			case GREATER_EQUAL:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::greater_equal_precision;
				else if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::greater_equal;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::greater_equal_color;
				else //OperativeArray
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			case LOWER:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::lower_precision;
				else if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::lower;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::lower_color;
				else //OperativeArray
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			case LOWER_EQUAL:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opPrecision = CompareOperator<dataType>::lower_equal_precision;
				else if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::lower_equal;
				else if constexpr (std::is_same_v<dataType, LitColor>)
					comparisonOperator.opColor = CompareOperator<dataType>::lower_equal_color;
				else //OperativeArray
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			case INCREASED_BY:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRangePrecision = CompareOperator<dataType>::increased_precision;
				else if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRange = CompareOperator<dataType>::increased;
				else //OperativeArray
					comparisonOperator.opRange = CompareOperator<dataType>::dummy_range;
				break;
			case DECREASED_BY:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRangePrecision = CompareOperator<dataType>::decreased_precision;
				else if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRange = CompareOperator<dataType>::decreased;
				else //OperativeArray
					comparisonOperator.opRange = CompareOperator<dataType>::dummy_range;
				break;
			case BETWEEN:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRangePrecision = CompareOperator<dataType>::between_precision;
				else if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRange = CompareOperator<dataType>::between;
				else //OperativeArray
					comparisonOperator.opRange = CompareOperator<dataType>::dummy_range;
				break;
			case NOT_BETWEEN:
				if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRangePrecision = CompareOperator<dataType>::not_between_precision;
				else if constexpr (std::is_floating_point_v<dataType>)
					comparisonOperator.opRange = CompareOperator<dataType>::not_between;
				else //OperativeArray
					comparisonOperator.opRange = CompareOperator<dataType>::dummy_range;
				break;
			case AND:
				if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::And;
				else
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			case OR:
				if constexpr (std::is_integral_v<dataType>)
					comparisonOperator.opSimple = CompareOperator<dataType>::Or;
				else
					comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			default:
				comparisonOperator.opSimple = CompareOperator<dataType>::dummy;
				break;
			}
		}

		template<typename addressType, typename dataType> void primitiveUnknownInitial()
		{
			addressType addr;
			dataType val;
			DataAccess<dataType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<dataType>::readReversed : DataAccess<dataType>::read;

			for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
			{
				addr = offsetDump + _currentBaseAddress;
				val = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + offsetDump));
				_results.back().PushBackResult<addressType, dataType>(addr, val);
			}
		}

		template<typename addressType, typename dataType> void primitiveUnknownSuccessive()
		{
			addressType addr;
			dataType oldval;
			dataType readval;
			CompareOperator<dataType> comparisonOperator;
			setUpComparasionOperator<dataType>(comparisonOperator);
			DataAccess<dataType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<dataType>::readReversed : DataAccess<dataType>::read;
			
			if (_condition == INCREASED_BY || _condition == DECREASED_BY)
			{
				const dataType increamentVal = parseKnownValue<dataType>(_primaryKnownValueStr, _iterationFlags & HEX);

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					oldval = _results[_counterIterationIndex].GetValueByRangeIndex<dataType>(_previousIterationRangeIndex, i);
					readval = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + addr - _currentBaseAddress));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readval, oldval, increamentVal))
							_results.back().PushBackResult<addressType, dataType>(addr, readval, oldval);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readval, oldval, increamentVal, _precision))
							_results.back().PushBackResult<addressType, dataType>(addr, readval, oldval);
					}
				}
			}
			else //EQUAL, UNEQUAL, GREATER, GREATER_EQUAL, LOWER, LOWER_EQUAL
			{
				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					oldval = _results[_counterIterationIndex].GetValueByRangeIndex<dataType>(_previousIterationRangeIndex, i);
					readval = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + addr - _currentBaseAddress));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readval, oldval))
							_results.back().PushBackResult<addressType, dataType>(addr, readval, oldval);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readval, oldval, _precision, false))
							_results.back().PushBackResult<addressType, dataType>(addr, readval, oldval);
					}
				}
			}
		}

		template<typename addressType, typename dataType> void primitiveKnownInitial()
		{
			addressType addr;
			dataType readVal;
			dataType primaryKnownVal = parseKnownValue<dataType>(_primaryKnownValueStr, _iterationFlags & HEX);
			CompareOperator<dataType> comparisonOperator;
			setUpComparasionOperator<dataType>(comparisonOperator);
			DataAccess<dataType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<dataType>::readReversed : DataAccess<dataType>::read;
			
			if (_condition == INCREASED_BY || _condition == DECREASED_BY || _condition == BETWEEN || _condition == NOT_BETWEEN)
			{
				dataType secondaryKnownVal = parseKnownValue<dataType>(_secondaryKnownValueStr, _iterationFlags & HEX);

				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					readVal = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + offsetDump));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, secondaryKnownVal))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, secondaryKnownVal, _precision))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal);
					}
				}
			}
			else //EQUAL, UNEQUAL, GREATER, GREATER_EQUAL, LOWER, LOWER_EQUAL
			{
				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					readVal = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + offsetDump));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, _precision, false))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal);
					}
				}
			}
		}

		template<typename addressType, typename dataType> void primitiveKnownSuccessive()
		{
			addressType addr;
			dataType readVal;
			dataType oldVal;
			dataType primaryKnownVal = parseKnownValue<dataType>(_primaryKnownValueStr, _iterationFlags & HEX);
			CompareOperator<dataType> comparisonOperator;
			setUpComparasionOperator<dataType>(comparisonOperator);
			DataAccess<dataType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<dataType>::readReversed : DataAccess<dataType>::read;

			if (_condition == INCREASED_BY || _condition == DECREASED_BY || _condition == BETWEEN || _condition == NOT_BETWEEN)
			{
				dataType secondaryKnownVal = parseKnownValue<dataType>(_primaryKnownValueStr, _iterationFlags & HEX);

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					oldVal = _results[_counterIterationIndex].GetValueByRangeIndex<dataType>(_previousIterationRangeIndex, i);
					readVal = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + addr - _currentBaseAddress));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, secondaryKnownVal))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal, oldVal);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, secondaryKnownVal, _precision))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal, oldVal);
					}
				}
			}
			else //EQUAL, UNEQUAL, GREATER, GREATER_EQUAL, LOWER, LOWER_EQUAL
			{
				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					oldVal = _results[_counterIterationIndex].GetValueByRangeIndex<dataType>(_previousIterationRangeIndex, i);
					readVal = byteReader(*reinterpret_cast<dataType*>(_currentDumpAddress + addr - _currentBaseAddress));

					if constexpr (std::is_integral_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal, oldVal);
					}
					else if constexpr (std::is_floating_point_v<dataType>)
					{
						if (comparisonOperator(readVal, primaryKnownVal, _precision, false))
							_results.back().PushBackResult<addressType, dataType>(addr, readVal, oldVal);
					}
				}
			}
		}

		template<typename addressType> void colorKnownInitial()
		{
			addressType addr;
			LitColor knownVal(_primaryKnownValueStr);
			LitColor readVal;
			readVal.SelectType(_subsidiaryDatatype);
			CompareOperator<LitColor> comparisonOperator;
			setUpComparasionOperator<LitColor>(comparisonOperator);

			switch (_subsidiaryDatatype)
			{
			case LitColor::RGB565:
			{
				DataAccess<uint16_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint16_t>::readReversed : DataAccess<uint16_t>::read;

				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					readVal = byteReader(*reinterpret_cast<uint16_t*>(_currentDumpAddress + offsetDump));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint16_t>(addr, readVal.GetRGB565());
				}
			}
			break;
			case LitColor::RGB5A3:
			{
				DataAccess<uint16_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint16_t>::readReversed : DataAccess<uint16_t>::read;

				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					readVal = byteReader(*reinterpret_cast<uint16_t*>(_currentDumpAddress + offsetDump));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint16_t>(addr, readVal.GetRGB5A3());
				}
			}
			break;
			case LitColor::RGBF: case LitColor::RGBAF:
			{
				DataAccess<float> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<float>::readReversed : DataAccess<float>::read;
				const int colorValueCount = (knownVal.UsesAlpha() ? 4 : 3);
				static float colorBuf[4];

				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize - 2 * sizeof(float); offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					for (int rgbaSelect = 0; rgbaSelect < colorValueCount; ++rgbaSelect)
						readVal.SetColorValue<float>(byteReader(*reinterpret_cast<float*>(_currentDumpAddress + offsetDump + rgbaSelect * sizeof(float))), rgbaSelect);

					if (comparisonOperator(readVal, knownVal, _precision))
					{
						for (int rgbaSelect = 0; rgbaSelect < colorValueCount; ++rgbaSelect)
							colorBuf[rgbaSelect] = readVal.GetColorValue<float>(rgbaSelect);

						_results.back().PushBackResultByPtr<addressType>(addr, reinterpret_cast<char*>(colorBuf));
					}
				}
			}
			break;
			default: //RGB888, RGBA8888
			{
				DataAccess<uint32_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint32_t>::readReversed : DataAccess<uint32_t>::read;

				for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
				{
					addr = _currentBaseAddress + offsetDump;
					readVal = byteReader(*reinterpret_cast<uint32_t*>(_currentDumpAddress + offsetDump));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint32_t>(addr, readVal.GetRGBA());
				}
			}
			}
		}

		template<typename addressType> void colorKnownSuccessive()
		{
			addressType addr;
			LitColor knownVal(_primaryKnownValueStr);
			LitColor readVal = knownVal;
			CompareOperator<LitColor> comparisonOperator;
			setUpComparasionOperator<LitColor>(comparisonOperator);

			switch (_subsidiaryDatatype)
			{
			case LitColor::RGB565:
			{
				DataAccess<uint16_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint16_t>::readReversed : DataAccess<uint16_t>::read;

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					readVal = byteReader(*reinterpret_cast<uint16_t*>(_currentDumpAddress + addr - _currentBaseAddress));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint16_t>(addr, readVal.GetRGB565(), _results[_counterIterationIndex].GetValueByRangeIndex<uint16_t>(_previousIterationRangeIndex, i));
				}
			}
			break;
			case LitColor::RGB5A3:
			{
				DataAccess<uint16_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint16_t>::readReversed : DataAccess<uint16_t>::read;

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					readVal = byteReader(*reinterpret_cast<uint16_t*>(_currentDumpAddress + addr - _currentBaseAddress));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint16_t>(addr, readVal.GetRGB5A3(), _results[_counterIterationIndex].GetValueByRangeIndex<uint16_t>(_previousIterationRangeIndex, i));
				}
			}
			break;
			case LitColor::RGBF: case LitColor::RGBAF:
			{
				DataAccess<float> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<float>::readReversed : DataAccess<float>::read;
				const int colorValueCount = (knownVal.UsesAlpha() ? 4 : 3);
				float colorBuf[4];

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);

					for (int rgbaSelect = 0; rgbaSelect < colorValueCount; ++rgbaSelect)
						readVal.SetColorValue<float>(byteReader(*reinterpret_cast<float*>(_currentDumpAddress + addr - _currentBaseAddress + rgbaSelect * sizeof(float))), rgbaSelect);

					if (comparisonOperator(readVal, knownVal, _precision))
					{
						for (int rgbaSelect = 0; rgbaSelect < colorValueCount; ++rgbaSelect)
							colorBuf[rgbaSelect] = readVal.GetColorValue<float>(rgbaSelect);

						_results.back().PushBackResultByPtr<addressType>(addr, reinterpret_cast<char*>(colorBuf), reinterpret_cast<char*>(_results[_counterIterationIndex].GetSpecificValuePtrByRangeIndex<float>(_previousIterationRangeIndex, colorValueCount * i)));
					}
				}
			}
			break;
			default:  //RGB888, RGBA8888
				DataAccess<uint32_t> byteReader;
				byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint32_t>::readReversed : DataAccess<uint32_t>::read;

				for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
				{
					addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);
					readVal = byteReader(*reinterpret_cast<uint32_t*>(_currentDumpAddress + addr - _currentBaseAddress));

					if (comparisonOperator(readVal, knownVal, _precision))
						_results.back().PushBackResult<addressType, uint32_t>(addr, readVal.GetRGBA(), _results[_counterIterationIndex].GetValueByRangeIndex<uint32_t>(_previousIterationRangeIndex, i));
				}
			}
		}

		template<typename addressType> void textKnownInitial()
		{
			addressType addr;
			MorphText knownVal;
			DataAccess<uint32_t> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<uint32_t>::readReversed : DataAccess<uint32_t>::read;
			MorphText knownValUTF8(_primaryKnownValueStr);
			int charCount;
			knownValUTF8.SetPrimaryEncoding(_subsidiaryDatatype);

			switch (_subsidiaryDatatype)
			{
				case MorphText::UTF16LE: case MorphText::UTF16BE: {
					charCount = knownValUTF8.GetString<std::wstring>(_subsidiaryDatatype).size();
					std::wstring buf(charCount, '\0');
					knownVal = knownValUTF8.GetString<std::wstring>(_subsidiaryDatatype);

					for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
					{
						memcpy(buf.data(), _currentDumpAddress + offsetDump, charCount);

						if (knownVal.Compare(buf.data(), _setupFlags & CASE_SENSITIVE, _subsidiaryDatatype))
						{
							addr = _currentBaseAddress + offsetDump;
							_results.back().PushBackResultByPtr<addressType>(addr, reinterpret_cast<char*>(buf.data()));
						}
					}
				} break;
				case MorphText::UTF32LE: case MorphText::UTF32BE: {
					charCount = knownValUTF8.GetString<std::u32string>(_subsidiaryDatatype).size();
					std::u32string buf(charCount, '\0');
					knownVal = knownValUTF8.GetString<std::u32string>(_subsidiaryDatatype);

					for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
					{
						memcpy(buf.data(), _currentDumpAddress + offsetDump, charCount);

						if (knownVal.Compare(buf.data(), _setupFlags & CASE_SENSITIVE, _subsidiaryDatatype))
						{
							addr = _currentBaseAddress + offsetDump;
							_results.back().PushBackResultByPtr<addressType>(addr, reinterpret_cast<char*>(buf.data()));
						}
					}
				} break;
				default: //ASCII, Shift-Jis, UTF-8, ISO-8859-X
				{
					charCount = knownValUTF8.GetString<std::string>(_subsidiaryDatatype).size();
					std::string buf(charCount, '\0');

					for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize; offsetDump += _alignment)
					{
						memcpy(buf.data(), _currentDumpAddress + offsetDump, charCount);

						if (knownValUTF8.Compare(buf.c_str(), _setupFlags & CASE_SENSITIVE, _subsidiaryDatatype))
						{
							addr = _currentBaseAddress + offsetDump;
							buf.append("\0");
							_results.back().PushBackResultByPtr<addressType>(addr, (char*)buf.c_str());
						}
					}
				}
			}
		}

		template<typename addressType, typename arrayType> void arrayKnownInitial()
		{
			addressType addr;
			OperativeArray<arrayType> knownValue(_primaryKnownValueStr);
			const uint32_t itemCount = knownValue.ItemCount();
			OperativeArray<arrayType> readArr(static_cast<arrayType>(0), itemCount);
			CompareOperator<OperativeArray<arrayType>> comparisonOperator;
			setUpComparasionOperator<OperativeArray<arrayType>>(comparisonOperator);
			DataAccess<arrayType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<arrayType>::readReversed : DataAccess<arrayType>::read;

			for (uint64_t offsetDump = 0; offsetDump < _currentDumpSize - (readArr.ItemCount()-1) * sizeof(arrayType); offsetDump += _alignment)
			{
				for (int index = 0; index < itemCount; ++index)
					readArr[index] = byteReader(*reinterpret_cast<arrayType*>(_currentDumpAddress + offsetDump + index*sizeof(arrayType)));

				if (comparisonOperator(knownValue, readArr))
				{
					addr = offsetDump + _currentBaseAddress;
					_results.back().PushBackResultByPtr<addressType>(addr, (char*)&readArr[0]);
				}
			}
		}

		template <typename addressType, typename arrayType> void arrayKnownSuccessive()
		{
			addressType addr;
			OperativeArray<arrayType> knownValue(_primaryKnownValueStr);
			const uint32_t itemCount = knownValue.ItemCount();
			CompareOperator<OperativeArray<arrayType>> comparisonOperator;
			setUpComparasionOperator<OperativeArray<arrayType>>(comparisonOperator);
			OperativeArray<arrayType>readArr(static_cast<arrayType>(0), itemCount);
			DataAccess<arrayType> byteReader;
			byteReader.reader = _setupFlags & BIG_ENDIAN ? DataAccess<arrayType>::readReversed : DataAccess<arrayType>::read;
			const uint16_t counterIterationIndex = _counterIteration - 1;

			for (uint64_t i = 0; i < _results[_counterIterationIndex].GetResultCountByRangeIndex(_previousIterationRangeIndex); ++i)
			{
				addr = _results[_counterIterationIndex].GetAddressByRangeIndex<addressType>(_previousIterationRangeIndex, i);

				for (int index = 0; index < itemCount; ++index)
					readArr[index] = byteReader(*reinterpret_cast<arrayType*>(_currentDumpAddress + addr - _currentBaseAddress + index * sizeof(arrayType)));

				if (comparisonOperator(knownValue, readArr))
					_results.back().PushBackResultByPtr<addressType>(addr, reinterpret_cast<char*>(&readArr[0]), reinterpret_cast<char*>(_results[_counterIterationIndex].GetSpecificValuePtrByRangeIndex<arrayType>(_previousIterationRangeIndex, itemCount * i)));
			}
		}

	public:
		static void SetUp(const std::wstring& resultsDir, const uint16_t superiorDatatype, const uint16_t subsidiaryDatatype, const uint8_t addressWidth, const uint16_t alignment = 4, const uint32_t setupFlags = 0);
		static void NewIteration(const uint8_t condition, const uint16_t counterIteration, std::string& primaryKnownValue, std::string& secondaryKnownValue, const float precision = 1.0f, const uint32_t iterationFlags = 0);
		static void ProcessNextRange(MemDump* range);
		static MemCompareResults& GetResults();
		static void Reset();
		static uint64_t GetResultCount();
		static uint64_t GetIterationCount();
	};
}
