#include "bankFactory.h"
#include "bank.h"

#include <QDebug>
#include <QHash>

#include "banks/bice.h"
#include "banks/chile.h"
#include "banks/santander.h"
// #include "banks/wise.h"
// #include "banks/estado.h"

std::unique_ptr<Bank> BankFactory::create(BankType type,
                                          const QString &typeAccount)
{
    switch (type) {
    case BankType::BICE:
        return std::make_unique<BICE>(typeAccount);
    case BankType::CHILE:
        return std::make_unique<Chile>(typeAccount);
    case BankType::SANTANDER:
        return std::make_unique<Santander>(typeAccount);
    // case BankType::WISE:
    //     return std::make_unique<Wise>(typeAccount);
    // case BankType::ESTADO:
    //     return std::make_unique<Estado>(typeAccount);
    case BankType::WISE:
    case BankType::ESTADO:
    case BankType::UNKNOWN:
    default:
        qDebug() << "Tipo de banco no soportado";
        return nullptr;
    }
}

std::unique_ptr<Bank> BankFactory::create(const QString &bankName,
                                          const QString &typeAccount)
{
    const BankType type = fromString(bankName);
    if (type == BankType::UNKNOWN) {
        qDebug() << "Banco no soportado:" << bankName;
        return nullptr;
    }
    return create(type, typeAccount);
}

std::unique_ptr<Bank> BankFactory::create(const QString &bankName,
                                          const QString &typeAccount,
                                          const QString &filePath)
{
    auto bank = create(bankName, typeAccount);
    if (bank)
        bank->filePath = filePath;
    return bank;
}

BankFactory::BankType BankFactory::fromString(const QString &bankName)
{
    static const QHash<QString, BankType> table = {
        { QStringLiteral("BICE"),      BankType::BICE      },
        { QStringLiteral("SANTANDER"), BankType::SANTANDER },
        { QStringLiteral("WISE"),      BankType::WISE      },
        { QStringLiteral("ESTADO"),    BankType::ESTADO    },
        { QStringLiteral("CHILE"),     BankType::CHILE     },
    };
    return table.value(bankName.trimmed().toUpper(), BankType::UNKNOWN);
}