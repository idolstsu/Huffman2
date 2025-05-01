#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <map>
#include <string>
#include <memory>
#include <clocale>
#include <bitset>
#include <iomanip>
#include <cstdint>

using namespace std;

struct HuffmanNode {
    char symbol;
    int count;
    shared_ptr<HuffmanNode> left, right;

    HuffmanNode(char sym, int cnt) : symbol(sym), count(cnt), left(nullptr), right(nullptr) {}
    HuffmanNode(int cnt, shared_ptr<HuffmanNode> l, shared_ptr<HuffmanNode> r)
        : symbol('\0'), count(cnt), left(l), right(r) {
    }
};

struct NodeCompare {
    bool operator()(const shared_ptr<HuffmanNode>& a, const shared_ptr<HuffmanNode>& b) const {
        return a->count > b->count;
    }
};

map<char, int> countSymbols(const string& text) {
    map<char, int> counts;
    for (char c : text) counts[c]++;
    return counts;
}

shared_ptr<HuffmanNode> createHuffmanTree(const map<char, int>& counts) {
    priority_queue<shared_ptr<HuffmanNode>, vector<shared_ptr<HuffmanNode>>, NodeCompare> pq;

    for (const auto& entry : counts) {
        pq.push(make_shared<HuffmanNode>(entry.first, entry.second));
    }

    while (pq.size() > 1) {
        auto left = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();
        int total = left->count + right->count;
        pq.push(make_shared<HuffmanNode>(total, left, right));
    }

    return pq.top();
}

void generateCodes(const shared_ptr<HuffmanNode>& node, string code, map<char, string>& codes) {
    if (!node) return;

    if (!node->left && !node->right) {
        codes[node->symbol] = code;
        return;
    }

    generateCodes(node->left, code + "0", codes);
    generateCodes(node->right, code + "1", codes);
}

string encodeText(const string& text, const map<char, string>& codes) {
    string result;
    for (char c : text) result += codes.at(c);
    return result;
}

void saveCompressed(ofstream& out, const map<char, string>& codes, const string& encoded) {
    size_t codeCount = codes.size();
    out.write(reinterpret_cast<const char*>(&codeCount), sizeof(codeCount));

    for (const auto& entry : codes) {
        out.put(entry.first);
        uint8_t len = static_cast<uint8_t>(entry.second.length());
        out.put(static_cast<char>(len));

        uint32_t packed = 0;
        for (char bit : entry.second) {
            packed = (packed << 1) | (bit == '1' ? 1 : 0);
        }
        out.write(reinterpret_cast<const char*>(&packed), sizeof(packed));
    }

    uint8_t byte = 0;
    int bitsLeft = 7;

    for (char bit : encoded) {
        if (bit == '1') byte |= (1 << bitsLeft);
        if (--bitsLeft < 0) {
            out.put(static_cast<char>(byte));
            byte = 0;
            bitsLeft = 7;
        }
    }

    if (bitsLeft != 7) out.put(static_cast<char>(byte));
}

void compressFile(ifstream& in, ofstream& out) {
    string text((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    auto counts = countSymbols(text);
    auto tree = createHuffmanTree(counts);

    map<char, string> codes;
    generateCodes(tree, "", codes);

    string encoded = encodeText(text, codes);
    saveCompressed(out, codes, encoded);

    cout << "Compression complete:\n";
    cout << "Original: " << text.size() << " bytes\n";
    cout << "Compressed: " << out.tellp() << " bytes\n";
}

void loadCompressed(ifstream& in, map<char, string>& codes, string& encoded) {
    size_t codeCount;
    in.read(reinterpret_cast<char*>(&codeCount), sizeof(codeCount));

    for (size_t i = 0; i < codeCount; ++i) {
        char sym;
        in.get(sym);
        uint8_t len;
        in.get(reinterpret_cast<char&>(len));
        uint32_t packed;
        in.read(reinterpret_cast<char*>(&packed), sizeof(packed));

        string code;
        for (int j = len - 1; j >= 0; --j) {
            code += ((packed >> j) & 1) ? '1' : '0';
        }
        codes[sym] = code;
    }

    char byte;
    while (in.get(byte)) {
        for (int i = 7; i >= 0; --i) {
            encoded += ((byte >> i) & 1) ? '1' : '0';
        }
    }
}

string decodeText(const string& encoded, const shared_ptr<HuffmanNode>& root) {
    string result;
    auto node = root;

    for (char bit : encoded) {
        node = (bit == '0') ? node->left : node->right;
        if (!node->left && !node->right) {
            result += node->symbol;
            node = root;
        }
    }

    return result;
}

void decompressFile(ifstream& in, ofstream& out) {
    map<char, string> codes;
    string encoded;
    loadCompressed(in, codes, encoded);

    auto root = make_shared<HuffmanNode>('\0', 0);
    for (const auto& entry : codes) {
        auto node = root;
        for (char bit : entry.second) {
            if (bit == '0') {
                if (!node->left) node->left = make_shared<HuffmanNode>('\0', 0);
                node = node->left;
            }
            else {
                if (!node->right) node->right = make_shared<HuffmanNode>('\0', 0);
                node = node->right;
            }
        }
        node->symbol = entry.first;
    }

    string text = decodeText(encoded, root);
    out << text;
}

int main() {
    setlocale(LC_ALL, "rus");

    string filename;
    cout << "Enter filename (exp.txt to compress | encoded.txt to decompress): ";
    cin >> filename;

    ifstream in(filename, ios::binary);
    if (!in) {
        cout << "Error opening file!" << endl;
        return 1;
    }

    string choice;
    cout << "Enter '1' to compress or '2' to decompress: ";
    cin >> choice;

    if (choice == "1") {
        ofstream out("encoded.txt", ios::binary);
        if (!out) {
            cout << "Error creating output file!" << endl;
            return 1;
        }

        cout << "Compressing..." << endl;
        compressFile(in, out);
        cout << "Done!" << endl;
    }
    else if (choice == "2") {
        ofstream out("decoded.txt", ios::binary);
        if (!out) {
            cout << "Error creating output file!" << endl;
            return 1;
        }

        cout << "Decompressing..." << endl;
        decompressFile(in, out);
        cout << "Done!" << endl;
    }
    else {
        cout << "Invalid choice!" << endl;
        return 1;
    }

    in.close();
    return 0;
}