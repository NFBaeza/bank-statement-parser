#ifndef SANTANDER_H
#define SANTANDER_H

#include "bank.h"

class Santander : public Bank
{
public:
    explicit Santander(const QString &typeAccount);
    Santander(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

#endif // SANTANDER_H
