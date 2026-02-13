#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>

struct Recruit {
    std::string name;
    std::string birthDate;
    std::vector<std::pair<std::string, std::string>> doctorRecords;
    
    bool isFitForService() const {
        for (const auto& record : doctorRecords) {
            if (record.second == "A") {
                return true;
            }
        }
        return false;
    }
    
    void print() const {
        std::cout << "Имя: " << name << ", Дата рождения: " << birthDate;
        std::cout << ", Записи врачей: ";
        for (const auto& record : doctorRecords) {
            std::cout << "(" << record.first << ": " << record.second << ") ";
        }
        std::cout << ", Пригоден: " << (isFitForService() ? "Да" : "Нет") << std::endl;
    }
};

std::mutex dataMutex;
std::vector<Recruit> suitableRecruits;
std::vector<Recruit> allRecruits;

std::vector<Recruit> readRecruitsFromFile(const std::string& filename) {
    std::vector<Recruit> recruits;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка открытия файла: " << filename << std::endl;
        return recruits;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Recruit recruit;
        
        if (!(iss >> recruit.name >> recruit.birthDate)) {
            continue;
        }
        
        std::string specialty, category;
        while (iss >> specialty >> category) {
            recruit.doctorRecords.emplace_back(specialty, category);
        }
        
        recruits.push_back(recruit);
    }
    
    file.close();
    return recruits;
}

//однопоточная фильтрация
std::vector<Recruit> filterRecruitsSingleThread(const std::vector<Recruit>& recruits) {
    std::vector<Recruit> suitable;
    
    for (const auto& recruit : recruits) {
        if (recruit.isFitForService()) {
            suitable.push_back(recruit);
        }
    }
    
    return suitable;
}

void filterRecruitsRange(int start, int end) {
    std::vector<Recruit> localSuitable;
    
    for (int i = start; i < end && i < allRecruits.size(); ++i) {
        if (allRecruits[i].isFitForService()) {
            localSuitable.push_back(allRecruits[i]);
        }
    }
    
    std::lock_guard<std::mutex> lock(dataMutex);
    suitableRecruits.insert(suitableRecruits.end(), localSuitable.begin(), localSuitable.end());
}

//многопоточная фильтрация
std::vector<Recruit> filterRecruitsMultiThread(const std::vector<Recruit>& recruits, int numThreads = 4) {
    suitableRecruits.clear();
    allRecruits = recruits;
    
    std::vector<std::thread> threads;
    int chunkSize = recruits.size() / numThreads;
    
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? recruits.size() : (i + 1) * chunkSize;
        threads.emplace_back(filterRecruitsRange, start, end);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    return suitableRecruits;
}

void generateTestData(const std::string& filename, int numRecruits) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка создания файла: " << filename << std::endl;
        return;
    }
    
    std::vector<std::string> names = {"Иванов", "Петров", "Сидоров", "Кузнецов", "Смирнов", 
                                      "Попов", "Васильев", "Павлов", "Семенов", "Голубев"};
    std::vector<std::string> specialties = {"терапевт", "хирург", "окулист", "лор", "психиатр"};
    std::vector<std::string> categories = {"A", "Бв", "Б", "В", "Г", "Д"};
    
    std::srand(std::time(nullptr));
    
    for (int i = 0; i < numRecruits; ++i) {
        std::string name = names[std::rand() % names.size()] + "_" + std::to_string(i);
        
        int year = 1990 + std::rand() % 15;
        int month = 1 + std::rand() % 12;
        int day = 1 + std::rand() % 28;
        std::string birthDate = std::to_string(year) + "." + 
                                (month < 10 ? "0" : "") + std::to_string(month) + "." + 
                                (day < 10 ? "0" : "") + std::to_string(day);
        
        file << name << " " << birthDate;
        
        int numRecords = 1 + std::rand() % 3;
        for (int j = 0; j < numRecords; ++j) {
            std::string specialty = specialties[std::rand() % specialties.size()];
            std::string category = categories[std::rand() % categories.size()];
            file << " " << specialty << " " << category;
        }
        
        file << std::endl;
    }
    
    file.close();
    std::cout << "Сгенерировано " << numRecruits << " записей в файле " << filename << std::endl;
}

int main() {
    std::string filename = "recruits.txt";
    generateTestData(filename, 1000000);
    
    std::cout << "\nЧтение данных из файла..." << std::endl;
    auto recruits = readRecruitsFromFile(filename);
    std::cout << "Прочитано " << recruits.size() << " записей о призывниках" << std::endl;
    
    std::cout << "\n=== Однопоточная обработка ===" << std::endl;
    auto startSingle = std::chrono::high_resolution_clock::now();
    auto suitableSingle = filterRecruitsSingleThread(recruits);
    auto endSingle = std::chrono::high_resolution_clock::now();
    auto durationSingle = std::chrono::duration_cast<std::chrono::milliseconds>(endSingle - startSingle);
    
    std::cout << "Время обработки: " << durationSingle.count() << " мс" << std::endl;
    std::cout << "Найдено пригодных призывников: " << suitableSingle.size() << std::endl;
    
    std::cout << "\n=== Многопоточная обработка ===" << std::endl;
    auto startMulti = std::chrono::high_resolution_clock::now();
    auto suitableMulti = filterRecruitsMultiThread(recruits, 4);
    auto endMulti = std::chrono::high_resolution_clock::now();
    auto durationMulti = std::chrono::duration_cast<std::chrono::milliseconds>(endMulti - startMulti);
    
    std::cout << "Время обработки: " << durationMulti.count() << " мс" << std::endl;
    std::cout << "Найдено пригодных призывников: " << suitableMulti.size() << std::endl;
    
    if (suitableSingle.size() == suitableMulti.size()) {
        std::cout << "\nРезультаты обработки совпадают!" << std::endl;
    } else {
        std::cout << "\nВнимание: результаты не совпадают!" << std::endl;
    }
    
    double speedup = static_cast<double>(durationSingle.count()) / durationMulti.count();
    std::cout << "Ускорение: " << speedup << "x" << std::endl;
    
    if (!suitableSingle.empty()) {
        std::cout << "\n=== Первые 5 пригодных призывников ===" << std::endl;
        int count = std::min(5, static_cast<int>(suitableSingle.size()));
        for (int i = 0; i < count; ++i) {
            std::cout << i + 1 << ". ";
            suitableSingle[i].print();
        }
    }
    
    std::cout << "\n=== Статистика ===" << std::endl;
    std::cout << "Всего призывников: " << recruits.size() << std::endl;
    std::cout << "Пригодных: " << suitableSingle.size() << std::endl;
    std::cout << "Процент пригодных: " 
              << (static_cast<double>(suitableSingle.size()) / recruits.size() * 100) 
              << "%" << std::endl;
    
    return 0;
}
