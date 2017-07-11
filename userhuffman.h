#pragma once


struct record
{
	int frequency;
	int id;
};



struct treeItem
{
	int frequency;
	int left;
	int right;
	int bits;
};



struct lenghtRecord
{
	lenghtRecord(int val, int load)
	{
		value = uint8_t(val);
		payLoad = uint8_t(load);
	}
	uint8_t value;
	uint8_t payLoad;
};


std::vector<lenghtRecord> FromLengths(const std::vector<int> lengths, std::vector<int>& freqs);
int calcLengths(const std::vector<int>& symbolFreqs, std::vector<int>& lengths, int maxLength);

