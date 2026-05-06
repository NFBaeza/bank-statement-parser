#ifndef PDFPARSER_BANKFACTORY_H
#define PDFPARSER_BANKFACTORY_H

#include <memory>
#include <QString>

namespace pdfparser {

class Bank;

class BankFactory
{
public:
    enum class BankType {
        BICE,
        SANTANDER,
        WISE,
        ESTADO,
        CHILE,
        UNKNOWN
    };

    static std::unique_ptr<Bank> create(BankType type,
                                        const QString &typeAccount);

    static std::unique_ptr<Bank> create(const QString &bankName,
                                        const QString &typeAccount);

    static std::unique_ptr<Bank> create(const QString &bankName,
                                        const QString &typeAccount,
                                        const QString &filePath);

    static BankType fromString(const QString &bankName);
};

} // namespace pdfparser

#endif // PDFPARSER_BANKFACTORY_H
