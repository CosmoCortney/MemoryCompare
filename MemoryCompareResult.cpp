#include "MemoryCompareResult.hpp"
#include "MemoryCompare.hpp"
#include <filesystem>

std::wstring MemoryCompare::MemCompareResults::_path = L"";

MemoryCompare::MemCompareResults::MemCompareResults(std::wstring path, uint16_t iteration, uint8_t addressWidth, uint16_t valueWidth, uint16_t rangeCount, bool cached, bool zip)
{
	_path = path;
	_rangeCount = rangeCount;
	_iteration = iteration;
	_addressWidth = addressWidth;
	_valueWidth = valueWidth;
	_cached = cached;
	_zipped = zip;
}

//MemoryCompare::MemCompareResults::MemCompareResults(const MemCompareResults& other)
//{
//	*this = other;
//}

void MemoryCompare::MemCompareResults::createHeader(const uint32_t index)
{
	_fileHeaders.emplace_back();
	_fileHeaders[index].resize(16);
	*(uint64_t*)_fileHeaders[index].data() = _startingAdresses[index];
	*(uint64_t*)(_fileHeaders[index].data() + RESULT_COUNT) = _resultCounts[index];
	*(uint16_t*)(_fileHeaders[index].data() + ITERATION) = _iteration;
	*(uint16_t*)(_fileHeaders[index].data() + VALUE_WIDTH) = _valueWidth;
	*(char*)(_fileHeaders[index].data() + ADDRESS_WIDTH) = _addressWidth;
	*(uint64_t*)(_fileHeaders[index].data() + PLACEHOLDER) = 0;
}

void MemoryCompare::MemCompareResults::SetNewRange(const uint64_t startingAddress)
{
	++_rangeCount;
	_startingAdresses.push_back(startingAddress);
	_resultCounts.push_back(0);
	_indices.push_back(_totalResultCount);
}

int MemoryCompare::MemCompareResults::GetRangeIndexOfStartingAddress(const uint64_t startingAddress) const
{
	for (int i = 0; i < _rangeCount; ++i)
		if (_startingAdresses[i] == startingAddress)
			return i;

	return -1;
}

uint64_t MemoryCompare::MemCompareResults::GetTotalResultCount()
{
	return _totalResultCount;
}

uint64_t MemoryCompare::MemCompareResults::GetResultCountByRangeIndex(const uint32_t index)
{
	return _resultCounts[index];
}

uint64_t MemoryCompare::MemCompareResults::GetResultCountOfRange(const uint64_t startingAddress)
{
	for (int i = 0; i < _rangeCount; ++i)
		if (_startingAdresses[i] == startingAddress)
			return _resultCounts[i];

	return _resultCounts[0];
}

void MemoryCompare::MemCompareResults::Clear(const bool keepIterationCount)
{
	_fileHeaders.clear();
	_addresses.clear();
	_values.clear();
	_previousValues.clear();
	_resultCounts.clear();
	_startingAdresses.clear();
	_totalResultCount = 0;

	if(!keepIterationCount)
		_iteration = 0;
}

void MemoryCompare::MemCompareResults::SetValueWidth(const uint16_t width)
{
	_valueWidth = width;
}

void MemoryCompare::MemCompareResults::SetAddressWidth(const uint8_t width)
{
	_addressWidth = width;
}

bool MemoryCompare::MemCompareResults::SaveResults(uint32_t rangeIndex, bool zipped)
{
	if (rangeIndex == -1)
		return false;

	createHeader(rangeIndex);
	std::wstring path = _path + std::to_wstring(_iteration-1);
	std::filesystem::create_directories(path);
	path.append(L"\\" + std::to_wstring(rangeIndex) + L".bin");

	if (!SaveBinary(path, _fileHeaders[rangeIndex].data(), HEADER_SIZE, false/*, zipped*/))
		return false;

	if (_addresses.size() == 0)
		return true;

	if (!SaveBinary(path, _addresses.data(), _resultCounts[rangeIndex] * _addressWidth, true/*, zipped*/))
		return false;

	if (!SaveBinary(path, _values.data(), _resultCounts[rangeIndex] * _valueWidth, true/*, zipped*/))
		return false;

	if(_iteration > 1)
		if (!SaveBinary(path, _previousValues.data(), _resultCounts[rangeIndex] * _valueWidth, true/*, zipped*/))
			return false;

	return true;
}

void MemoryCompare::MemCompareResults::ClearResultsDir(const int iterationIndex, const bool removeIterationDir)
{
	if (iterationIndex < 0)
	{
		for (const auto& entry : std::filesystem::directory_iterator(_path))
			std::filesystem::remove_all(entry.path());
	}
	else 
	{
		for (const auto& entry : std::filesystem::directory_iterator(_path + std::to_wstring(iterationIndex)))
			std::filesystem::remove_all(entry.path());

		if (removeIterationDir)
		{
			std::filesystem::remove(_path + std::to_wstring(iterationIndex));
		}
	}
}

void MemoryCompare::MemCompareResults::SetResultsDir(const std::wstring& dir)
{
	_path = dir;
}

bool MemoryCompare::MemCompareResults::LoadResults(bool zipped)
{
	Clear(true);
	std::wstring path = _path + std::to_wstring(_iteration-1);

	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		//std::cout << entry.path() << std::endl;

		uint64_t readSize = HEADER_SIZE;
		_fileHeaders.emplace_back(readSize, 0);
		static int index = _fileHeaders.size() - 1;
		static uint64_t readPos = 0;

		if (!LoadBinary(entry.path(), _fileHeaders.back().data(), readSize, readPos))
			return false;

		_startingAdresses.push_back(*(uint64_t*)_fileHeaders[index].data());
		_resultCounts.push_back(*(uint64_t*)(_fileHeaders[index].data() + RESULT_COUNT));
		_totalResultCount += _resultCounts.back();

		if (index == 0)
		{
			_addressWidth = *(_fileHeaders[index].data() + ADDRESS_WIDTH);
			_valueWidth = *(_fileHeaders[index].data() + VALUE_WIDTH);
			_iteration = *(uint16_t*)(_fileHeaders[index].data() + ITERATION);
		}

		_addresses.emplace_back();
		readSize = _addressWidth * _resultCounts.back();
		_addresses.resize(_addressWidth * _resultCounts.back());
		readPos = HEADER_SIZE;

		if (!LoadBinary(entry.path(), _addresses.data(), readSize, readPos))
			return false;


		_values.emplace_back();
		readPos += readSize;
		readSize = _valueWidth * _resultCounts.back();
		_values.resize(_valueWidth * _resultCounts.back());

		if (!LoadBinary(entry.path(), _values.data(), readSize, readPos))
			return false;

		if (_iteration)
		{
			_previousValues.emplace_back();
			readPos += readSize;
			_previousValues.resize(_valueWidth * _resultCounts.back());

			if (!LoadBinary(entry.path(), _previousValues.data(), readSize, readPos))
				return false;
		}
	}

	return true;
}

uint16_t MemoryCompare::MemCompareResults::GetValueWidth() const
{
	return _valueWidth;
}

uint16_t MemoryCompare::MemCompareResults::GetRangeCount() const
{
	return _rangeCount;
}

void MemoryCompare::MemCompareResults::operator=(const MemCompareResults& other)
{
	_addresses = other._addresses;
	_values = other._values;
	_previousValues = other._previousValues;
	_startingAdresses = other._startingAdresses;
	_resultCounts = other._resultCounts;
	_fileHeaders = other._fileHeaders;
	_path = other._path;
	_rangeCount = other._rangeCount;
	_totalResultCount = other._totalResultCount;
	_addressWidth = other._addressWidth;
	_valueWidth = other._valueWidth;
	_fileSize = other._fileSize;
	_iteration = other._iteration;
	_cached = other._cached;
	_zipped = other._zipped;
}
