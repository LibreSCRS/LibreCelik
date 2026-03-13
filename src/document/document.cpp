#include "document.h"
#include "document/eid/eid.h"
#include "document/vehicle/vehicle.h"
#include "document/health/health.h"
#include "document/pks/pks.h"
#include <eidcard/eidcard.h>
#include <vehiclecard/vehiclecard.h>
#include <healthcard/healthcard.h>
#include <pkscard/pkscard.h>

Document::Document(QWidget* parent) : QWidget(parent) {}

Document* Document::CreateDocument(const std::string& reader, QWidget* parent)
{
    // Lightweight probe — checks ATR / AID without opening a full session
    if (eidcard::EIdCard::probe(reader))
        return new EId(reader, parent);

    if (vehiclecard::VehicleCard::probe(reader))
        return new Vehicle(reader, parent);

    if (healthcard::HealthCard::probe(reader))
        return new Health(reader, parent);

    if (pkscard::PKSCard::probe(reader))
        return new Pks(reader, parent);

    return nullptr;
}
