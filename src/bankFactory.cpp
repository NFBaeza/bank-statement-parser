#include "bankFactory.h"
#include "bank.h"

#include <QDebug>
#include <QHash>

#include "banks/bice.h"
#include "banks/chile.h"
#include "banks/santander.h"
// #include "banks/wise.h"
// #include "banks/estado.h"

namespace pdfparser {

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
    // Keyword scan: the parent app may pass arbitrary text such as
    //   "Banco de Chile - Cuenta Corriente"  or  "Santander Chile, abril".
    // We match the FIRST keyword that appears in the input, preferring
    // longer keywords so "Santander Chile" resolves to SANTANDER (the
    // entity) rather than CHILE.
    struct Keyword { const char *needle; BankType type; };
    static const Keyword keywords[] = {
        // Order: longest first → prevents short keywords from shadowing
        // longer entity names that contain them.
        { "santander", BankType::SANTANDER },
        { "estado",    BankType::ESTADO    },
        { "chile",     BankType::CHILE     },
        { "bice",      BankType::BICE      },
        { "wise",      BankType::WISE      },
    };

    const QString hay = bankName.toLower();
    for (const auto &kw : keywords) {
        if (hay.contains(QLatin1String(kw.needle)))
            return kw.type;
    }
    return BankType::UNKNOWN;
}

} // namespace pdfparser