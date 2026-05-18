// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// This file exists solely to provide //% source-text comments for lupdate.
// It registers the English source strings for all qtTrId() IDs used in the
// project so that lupdate can populate <source> in the .ts files without
// clearing existing translations.
//
// The function is never called at runtime; it is compiled away by the
// optimiser. When adding a new qtTrId("lc-xxx") call anywhere in the project,
// add the corresponding QT_TRID_NOOP entry here so lupdate keeps working.

#include <QtCore/qglobal.h>
// qttranslation.h was introduced in Qt 6.5; use QCoreApplication as fallback.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtCore/qttranslation.h>
#else
#include <QtCore/QCoreApplication>
#endif

// QT_TRID_NOOP(id) expands to the bare string literal `id`, so every statement
// inside this function is an unused-value expression by design.
QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wunused-value")
QT_WARNING_DISABLE_GCC("-Wunused-value")

[[maybe_unused]] static void _translationsCatalog()
{
    // --- main window ---
    //% "<b>LibreCelik</b> :: Version %1 :: <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL-3.0-or-later</a>
    //:: <a href=\"https://github.com/LibreSCRS/LibreCelik\">github.com/LibreSCRS/LibreCelik</a>"
    QT_TRID_NOOP("lc-main-about-librecelik");
    //% "Card in reader is not supported."
    QT_TRID_NOOP("lc-reader-unsupported-card");
    //% "Card type %1 is not supported (no driver match)."
    QT_TRID_NOOP("lc-reader-unsupported-card-with-atr");
    //% "Reading card..."
    QT_TRID_NOOP("lc-reading-card");
    //% "<b>LibreMiddleware</b> :: Version %1 :: <a
    // href=\"https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html\">LGPL-2.1-or-later</a> :: <a
    // href=\"https://github.com/LibreSCRS/LibreMiddleware\">github.com/LibreSCRS/LibreMiddleware</a>"
    QT_TRID_NOOP("lc-main-about-libremiddleware");
    //% "♥ Support this project — <a href=\"https://librescrs.github.io/donate\">librescrs.github.io/donate</a>"
    QT_TRID_NOOP("lc-main-about-donate");

    // --- about dialog ---
    //% "Libre Čelik"
    QT_TRID_NOOP("lc-about-app-name");
    //% "Open-source smart card reader and digital signing tool"
    QT_TRID_NOOP("lc-about-description");
    //% "Copyright (c) 2024-present hirashix0"
    QT_TRID_NOOP("lc-about-copyright");
    //% "If you find this software useful, please consider supporting its development"
    QT_TRID_NOOP("lc-about-donate-motivation");
    //% "♥ Donate"
    QT_TRID_NOOP("lc-about-donate-button");
    //% "Authors"
    QT_TRID_NOOP("lc-about-credits-authors");
    //% "Author & Maintainer"
    QT_TRID_NOOP("lc-about-credits-role-maintainer");
    //% "GNU General Public License v3.0 or later"
    QT_TRID_NOOP("lc-about-license-gpl");
    //% "GNU Lesser General Public License v2.1 or later"
    QT_TRID_NOOP("lc-about-license-lgpl");
    //% "Apache License 2.0"
    QT_TRID_NOOP("lc-about-license-apache");
    //% "statically linked"
    QT_TRID_NOOP("lc-about-license-static");
    //% "bundled subset"
    QT_TRID_NOOP("lc-about-license-bundled");
    //% "SIL Open Font License 1.1"
    QT_TRID_NOOP("lc-about-license-ofl");
    //% "LGPL-2.1-or-later — statically linked, modified fork"
    QT_TRID_NOOP("lc-about-license-opensc");
    //% "MIT — vendored"
    QT_TRID_NOOP("lc-about-license-json");
    //% "MIT — vendored"
    QT_TRID_NOOP("lc-about-license-miniz");
    //% "zlib license — transitive dependency"
    QT_TRID_NOOP("lc-about-license-zlib");
    // lupdate //% markers must stay single-line; clang-format would wrap them.
    // clang-format off
    //% "The complete corresponding source code is publicly available at <a href=\"https://github.com/LibreSCRS/LibreMiddleware\">github.com/LibreSCRS/LibreMiddleware</a> and <a href=\"https://github.com/LibreSCRS/LibreCelik\">github.com/LibreSCRS/LibreCelik</a>. This offer is valid for as long as we distribute this software."
    // clang-format on
    QT_TRID_NOOP("lc-about-source-offer");
    //% "About"
    QT_TRID_NOOP("lc-about-tab-about");
    //% "Credits"
    QT_TRID_NOOP("lc-about-tab-credits");
    //% "License"
    QT_TRID_NOOP("lc-about-tab-license");
    //% "GitHub"
    QT_TRID_NOOP("lc-about-link-github");
    //% "Website"
    QT_TRID_NOOP("lc-about-link-website");
    //% "Version %1"
    QT_TRID_NOOP("lc-about-version");

    // --- generic ---
    //% "Unavailable"
    QT_TRID_NOOP("lc-doc-unavailable");

    // --- eID reader errors ---
    //% "Failed to connect to card."
    // --- eID print document field labels ---
    //% "ELECTRONIC ID CARD READER: DATA PRINTING"
    QT_TRID_NOOP("lc-eid-doc-title");
    //% "Foreigner id"
    QT_TRID_NOOP("lc-eid-doc-foreigner-id");
    //% "Printing date"
    QT_TRID_NOOP("lc-eid-doc-printing-date");
    //% "Foreigner Data"
    QT_TRID_NOOP("lc-eid-foreigner-data");
    //% "Citizen Data"
    QT_TRID_NOOP("lc-eid-citizen-data");
    //% "Surname"
    QT_TRID_NOOP("lc-eid-doc-surname");
    //% "Name"
    QT_TRID_NOOP("lc-eid-doc-name");
    //% "Parent name"
    QT_TRID_NOOP("lc-eid-doc-parent-name");
    //% "Nationality"
    QT_TRID_NOOP("lc-eid-doc-nationality");
    //% "Date of birth"
    QT_TRID_NOOP("lc-eid-doc-date-birth");
    //% "Place of birth"
    QT_TRID_NOOP("lc-eid-doc-place-birth");
    //% "Status of foreigner"
    QT_TRID_NOOP("lc-eid-doc-foreigner-status");
    //% "Date of address change"
    QT_TRID_NOOP("lc-eid-doc-address-change-date");
    //% "Gender"
    QT_TRID_NOOP("lc-eid-doc-gender");
    //% "Document data"
    QT_TRID_NOOP("lc-eid-doc-document-data");
    //% "Document issuer"
    QT_TRID_NOOP("lc-eid-doc-issuer");
    //% "Document number"
    QT_TRID_NOOP("lc-eid-doc-number");
    //% "Date of issuance"
    QT_TRID_NOOP("lc-eid-doc-issuance-date");
    //% "Valid to"
    QT_TRID_NOOP("lc-eid-doc-valid-to");

    // --- eID UI labels ---
    //% "Certificates"
    QT_TRID_NOOP("lc-eid-tree-certificates");
    //% "PIN"
    QT_TRID_NOOP("lc-eid-tree-pin");
    //% "Serbian eID"
    QT_TRID_NOOP("lc-eid-title-serbian");
    //% "Serbian eID"
    QT_TRID_NOOP("lc-eid-title");
    //% "Serbian eID (Foreigner)"
    QT_TRID_NOOP("lc-eid-title-foreigner");
    //% "JMBG"
    QT_TRID_NOOP("lc-eid-label-jmbg");
    //% "EBS"
    QT_TRID_NOOP("lc-eid-label-ebs");
    //% "Identity card"
    QT_TRID_NOOP("lc-eid-label-address");
    //% "Address"
    QT_TRID_NOOP("lc-eid-label-address-foreigner");
    //% "Given Name"
    QT_TRID_NOOP("lc-eid-label-given-name");
    //% "Surname"
    QT_TRID_NOOP("lc-eid-label-surname");
    //% "Date of Birth"
    QT_TRID_NOOP("lc-eid-label-date-of-birth");
    //% "Sex"
    QT_TRID_NOOP("lc-eid-label-sex");
    //% "Nationality"
    QT_TRID_NOOP("lc-eid-label-nationality");
    //% "Place of Birth"
    QT_TRID_NOOP("lc-eid-label-place-of-birth");
    //% "Parent Name"
    QT_TRID_NOOP("lc-eid-label-parent-name");
    //% "Date of Address Change"
    QT_TRID_NOOP("lc-eid-label-address-date");
    //% "Document Type"
    QT_TRID_NOOP("lc-eid-label-document-type");
    //% "Document Serial Number"
    QT_TRID_NOOP("lc-eid-label-document-serial-number");
    //% "Document"
    QT_TRID_NOOP("lc-eid-label-document");
    //% "Issuing Authority"
    QT_TRID_NOOP("lc-eid-label-issuing-authority");
    //% "Document Reg. No."
    QT_TRID_NOOP("lc-eid-label-doc-reg-no");
    //% "Issuing Date"
    QT_TRID_NOOP("lc-eid-label-issuing-date");
    //% "Expiry Date"
    QT_TRID_NOOP("lc-eid-label-expiry-date");
    //% "Card Certificate"
    QT_TRID_NOOP("lc-eid-label-card-verification");
    //% "Fixed Data"
    QT_TRID_NOOP("lc-eid-label-fixed-verification");
    //% "Variable Data"
    QT_TRID_NOOP("lc-eid-label-variable-verification");
    //% "can sign"
    QT_TRID_NOOP("lc-eid-pin-blocked");
    //% "%1 tries remaining"
    QT_TRID_NOOP("lc-eid-pin-tries-remaining");
    //% "?"
    QT_TRID_NOOP("lc-eid-pin-unknown");
    //% "View Certificate"
    QT_TRID_NOOP("lc-eid-menu-view-cert");
    //% "Change PIN"
    QT_TRID_NOOP("lc-eid-menu-change-pin");
    //% "Initialize PIN"
    QT_TRID_NOOP("lc-eid-menu-initialize-pin");
    //% "Transport PIN (not initialized)"
    QT_TRID_NOOP("lc-eid-pin-transport");
    //% "Print Document"
    QT_TRID_NOOP("lc-eid-print-title");
    //% "Status of Foreigner"
    QT_TRID_NOOP("lc-eid-label-status-of-foreigner");

    // --- Change PIN dialog ---
    //% "Initialize %1"
    QT_TRID_NOOP("lc-changepin-initialize-title");
    //% "Change %1"
    QT_TRID_NOOP("lc-changepin-change-title");
    //% "Transport PIN"
    QT_TRID_NOOP("lc-changepin-transport-placeholder");
    //% "PIN retries remaining: %1"
    QT_TRID_NOOP("lc-changepin-retries-remaining");
    //% "Current PIN"
    QT_TRID_NOOP("lc-changepin-current");
    //% "New PIN"
    QT_TRID_NOOP("lc-changepin-new");
    //% "Confirm new PIN"
    QT_TRID_NOOP("lc-changepin-confirm");
    //% "Changing PIN..."
    QT_TRID_NOOP("lc-changepin-changing");
    //% "PIN is blocked!"
    QT_TRID_NOOP("lc-changepin-blocked");
    //% "PIN changed successfully."
    QT_TRID_NOOP("lc-changepin-success");
    //% "Close"
    QT_TRID_NOOP("lc-changepin-close");
    //% "New PIN and confirmation do not match."
    QT_TRID_NOOP("lc-changepin-mismatch");
    //% "Show/Hide PIN"
    QT_TRID_NOOP("lc-changepin-show-hide");
    //% "PIN change failed."
    QT_TRID_NOOP("lc-changepin-failed");

    // --- Certificate viewer ---
    //% "Field"
    QT_TRID_NOOP("lc-cert-tree-field");
    //% "Value"
    QT_TRID_NOOP("lc-cert-tree-value");
    //% "Version"
    QT_TRID_NOOP("lc-cert-field-version");
    //% "Serial Number"
    QT_TRID_NOOP("lc-cert-field-serial-number");
    //% "Signature Algorithm"
    QT_TRID_NOOP("lc-cert-field-signature-algo");
    //% "Issuer"
    QT_TRID_NOOP("lc-cert-field-issuer");
    //% "Validity"
    QT_TRID_NOOP("lc-cert-field-validity");
    //% "Not Before"
    QT_TRID_NOOP("lc-cert-field-not-before");
    //% "Not After"
    QT_TRID_NOOP("lc-cert-field-not-after");
    //% "Subject"
    QT_TRID_NOOP("lc-cert-field-subject");
    //% "Public Key Info"
    QT_TRID_NOOP("lc-cert-field-public-key-info");
    //% "Algorithm"
    QT_TRID_NOOP("lc-cert-field-algorithm");
    //% "Key Size"
    QT_TRID_NOOP("lc-cert-field-key-size");
    //% "Extensions"
    QT_TRID_NOOP("lc-cert-field-extensions");
    //% " (Critical)"
    QT_TRID_NOOP("lc-cert-extension-critical");
    //% "Unknown"
    QT_TRID_NOOP("lc-cert-unknown");
    //% "Unknown"
    QT_TRID_NOOP("lc-cert-algorithm-unknown");
    //% "Certificate Viewer"
    QT_TRID_NOOP("lc-cert-dialog-title");
    //% "No certificates available."
    QT_TRID_NOOP("lc-cert-no-available");

    // --- Certificate verification results ---
    //% "Valid"
    QT_TRID_NOOP("lc-cert-verify-valid");
    //% "Unspecified error"
    QT_TRID_NOOP("lc-cert-verify-unspecified-error");
    //% "Unable to get issuer certificate"
    QT_TRID_NOOP("lc-cert-verify-no-issuer");
    //% "Certificate has expired"
    QT_TRID_NOOP("lc-cert-verify-expired");
    //% "Untrusted root certificate"
    QT_TRID_NOOP("lc-cert-verify-untrusted-root");
    //% "Trust unknown"
    QT_TRID_NOOP("lc-cert-verify-trust-unknown");
    //% "Unparseable certificate"
    QT_TRID_NOOP("lc-cert-parse-error");
    //% "Curve"
    QT_TRID_NOOP("lc-cert-field-curve");

    // --- Certificate KeyUsage / ExtendedKeyUsage / GeneralName labels ---
    //% "Certificate Sign"
    QT_TRID_NOOP("lc-cert-ku-key-cert-sign");
    //% "CRL Sign"
    QT_TRID_NOOP("lc-cert-ku-crl-sign");
    //% "Encipher Only"
    QT_TRID_NOOP("lc-cert-ku-encipher-only");
    //% "Decipher Only"
    QT_TRID_NOOP("lc-cert-ku-decipher-only");
    //% "Email"
    QT_TRID_NOOP("lc-cert-gn-rfc822");
    //% "DNS"
    QT_TRID_NOOP("lc-cert-gn-dns");
    //% "URI"
    QT_TRID_NOOP("lc-cert-gn-uri");
    //% "IP"
    QT_TRID_NOOP("lc-cert-gn-ip");
    //% "Directory"
    QT_TRID_NOOP("lc-cert-gn-directory");
    //% "Registered ID"
    QT_TRID_NOOP("lc-cert-gn-registered-id");
    //% "Other Name"
    QT_TRID_NOOP("lc-cert-gn-other");
    //% "X.400 Address"
    QT_TRID_NOOP("lc-cert-gn-x400");
    //% "EDI Party Name"
    QT_TRID_NOOP("lc-cert-gn-edi");

    // --- Error messages ---
    //% "No card connection available"
    QT_TRID_NOOP("lc-error-no-connection");
    //% "No plugin could read this card."
    QT_TRID_NOOP("lc-error-no-plugin");
    //% "Authentication failed"
    QT_TRID_NOOP("lc-error-auth-failed");

    // --- LibreMiddleware signing-failure LocalizedText keys ---
    // These IDs match Auth::ErrorKeys builders. LM ships an English fallback
    // in the LocalizedText value; LC registers the same keys here so the Qt
    // translator can resolve them when SigningResult::userMessage flows
    // through the GUI.
    //% "The timestamp authority server is unreachable."
    QT_TRID_NOOP("librescrs.error.sign.tsa_unreachable");
    //% "Certificate revocation data could not be fetched."
    QT_TRID_NOOP("librescrs.error.sign.revocation_fetch_failed");
    //% "Smart card communication error during signing."
    QT_TRID_NOOP("librescrs.error.sign.card_error");
    //% "The PIN entered for signing is incorrect."
    QT_TRID_NOOP("librescrs.error.sign.pin_verification_failed");
    //% "The signing request is invalid."
    QT_TRID_NOOP("librescrs.error.sign.invalid_request");
    //% "The signing request does not satisfy the configured policy."
    QT_TRID_NOOP("librescrs.error.sign.policy_violation");

    // --- eMRTD ---
    //% "Insert the MRZ data from the bottom of the document"
    QT_TRID_NOOP("lc-emrtd-insert-mrz-hint");

    // --- Health insurance card ---
    //% "Health Insurance Card"
    QT_TRID_NOOP("lc-health-title");
    //% "Failed to connect to card."
    QT_TRID_NOOP("lc-health-print-title");
    //% "HEALTH INSURANCE CARD READER: DATA PRINTING"
    QT_TRID_NOOP("lc-health-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-health-doc-printing-date");
    //% "Personal Data"
    QT_TRID_NOOP("lc-health-section-personal");
    //% "Insurance Data"
    QT_TRID_NOOP("lc-health-section-insurance");
    //% "Address"
    QT_TRID_NOOP("lc-health-section-address");
    //% "Insurance Carrier Data"
    QT_TRID_NOOP("lc-health-section-carrier");
    //% "Employer / Taxpayer"
    QT_TRID_NOOP("lc-health-section-taxpayer");
    //% "Name"
    QT_TRID_NOOP("lc-health-label-given-name");
    //% "Name (Latin)"
    QT_TRID_NOOP("lc-health-label-family-name");
    //% "Surname (Latin)"
    QT_TRID_NOOP("lc-health-label-parent-name");
    //% "Parent name (Latin)"
    QT_TRID_NOOP("lc-health-label-dob");
    //% "Gender"
    QT_TRID_NOOP("lc-health-label-gender");
    //% "Personal number (JMBG)"
    QT_TRID_NOOP("lc-health-label-jmbg");
    //% "Insurant number (LBO)"
    QT_TRID_NOOP("lc-health-label-lbo");
    //% "Insurer"
    QT_TRID_NOOP("lc-health-label-insurer");
    //% "Insurer ID"
    QT_TRID_NOOP("lc-health-label-insurer-id");
    //% "Card ID"
    QT_TRID_NOOP("lc-health-label-card-id");
    //% "Issue date"
    QT_TRID_NOOP("lc-health-label-issue-date");
    //% "Expiry date"
    QT_TRID_NOOP("lc-health-label-expiry");
    //% "Valid until"
    QT_TRID_NOOP("lc-health-label-valid-until");
    //% "Permanently valid"
    QT_TRID_NOOP("lc-health-label-permanently");
    //% "Street"
    QT_TRID_NOOP("lc-health-label-street");
    //% "Number"
    QT_TRID_NOOP("lc-health-label-number");
    //% "Apartment"
    QT_TRID_NOOP("lc-health-label-apartment");
    //% "Place"
    QT_TRID_NOOP("lc-health-label-place");
    //% "Municipality"
    QT_TRID_NOOP("lc-health-label-municipality");
    //% "Country"
    QT_TRID_NOOP("lc-health-label-country");
    //% "Carrier name"
    QT_TRID_NOOP("lc-health-label-carrier-name");
    //% "Carrier surname"
    QT_TRID_NOOP("lc-health-label-carrier-family-name");
    //% "Relationship"
    QT_TRID_NOOP("lc-health-label-carrier-relation");
    //% "Carrier ID"
    QT_TRID_NOOP("lc-health-label-carrier-id");
    //% "Carrier LBO"
    QT_TRID_NOOP("lc-health-label-carrier-lbo");
    //% "Employer name"
    QT_TRID_NOOP("lc-health-label-taxpayer-name");
    //% "Employer ID (PIB)"
    QT_TRID_NOOP("lc-health-label-taxpayer-id");
    //% "Employer residence"
    QT_TRID_NOOP("lc-health-label-taxpayer-res");
    //% "Activity code"
    QT_TRID_NOOP("lc-health-label-taxpayer-act");
    //% "Insurance basis"
    QT_TRID_NOOP("lc-health-label-insurance-basis");
    //% "Insurance description"
    QT_TRID_NOOP("lc-health-label-insurance-desc");
    //% "Insurance start"
    QT_TRID_NOOP("lc-health-label-insurance-start");
    //% "Family member"
    QT_TRID_NOOP("lc-health-label-family-member");
    //% "Yes"
    QT_TRID_NOOP("lc-health-val-yes");
    //% "No"
    QT_TRID_NOOP("lc-health-val-no");

    // --- Token section (shared by eID, Health, etc.) ---
    //% "Smart Card Token"
    QT_TRID_NOOP("lc-token-widget-title");
    //% "Token"
    QT_TRID_NOOP("lc-token-title");
    //% "Label"
    QT_TRID_NOOP("lc-pki-token-label");
    //% "Serial Number"
    QT_TRID_NOOP("lc-pki-token-serial");
    //% "Manufacturer"
    QT_TRID_NOOP("lc-pki-token-manufacturer");
    // Tree column headers
    //% "Object"
    QT_TRID_NOOP("lc-token-col-object");
    //% "Details"
    QT_TRID_NOOP("lc-token-col-details");
    // Key properties
    //% "Subject"
    QT_TRID_NOOP("lc-token-key-subject");
    //% "Algorithm"
    QT_TRID_NOOP("lc-token-key-algorithm");
    //% "Key usage"
    QT_TRID_NOOP("lc-token-key-usage");
    //% "Valid"
    QT_TRID_NOOP("lc-token-key-valid");
    //% "Private key"
    QT_TRID_NOOP("lc-token-key-private-key");
    // Key usage flags
    //% "Digital Signature"
    QT_TRID_NOOP("lc-token-ku-digital-signature");
    //% "Non-Repudiation"
    QT_TRID_NOOP("lc-token-ku-non-repudiation");
    //% "Key Encipherment"
    QT_TRID_NOOP("lc-token-ku-key-encipherment");
    //% "Data Encipherment"
    QT_TRID_NOOP("lc-token-ku-data-encipherment");
    //% "Key Agreement"
    QT_TRID_NOOP("lc-token-ku-key-agreement");

    // --- Shared UI ---
    //% "Print"
    QT_TRID_NOOP("lc-print-tooltip");

    // --- Vehicle registration ---
    //% "Vehicle Registration"
    // --- EU VRC (Vehicle Registration Certificate) ---
    //% "Vehicle Registration"
    QT_TRID_NOOP("lc-euvrc-title");
    //% "Print Document"
    QT_TRID_NOOP("lc-euvrc-print-title");
    //% "Registration"
    QT_TRID_NOOP("lc-euvrc-section-registration");
    //% "Vehicle"
    QT_TRID_NOOP("lc-euvrc-section-vehicle");
    //% "Engine & Technical"
    QT_TRID_NOOP("lc-euvrc-section-engine");
    //% "Holder"
    QT_TRID_NOOP("lc-euvrc-section-holder");
    //% "User"
    QT_TRID_NOOP("lc-euvrc-section-user");
    //% "National Extensions"
    QT_TRID_NOOP("lc-euvrc-section-national");
    //% "Owner"
    QT_TRID_NOOP("lc-euvrc-section-owner");
    //% "Surname or business name (C.2)"
    QT_TRID_NOOP("lc-euvrc-owner-name");

    // --- EU VRC header fields ---
    //% "Registration (A)"
    QT_TRID_NOOP("lc-euvrc-hdr-registration");
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-hdr-make");
    //% "Member State"
    QT_TRID_NOOP("lc-euvrc-hdr-member-state");
    //% "Valid to (H)"
    QT_TRID_NOOP("lc-euvrc-hdr-valid-to");

    // --- EU VRC registration field labels ---
    //% "Registration number (A)"
    QT_TRID_NOOP("lc-euvrc-reg-number");
    //% "Date of first registration (B)"
    QT_TRID_NOOP("lc-euvrc-reg-first-date");
    //% "Registration date (I)"
    QT_TRID_NOOP("lc-euvrc-reg-date");
    //% "Expiry date (H)"
    QT_TRID_NOOP("lc-euvrc-reg-expiry");
    //% "Document number"
    QT_TRID_NOOP("lc-euvrc-reg-doc-number");
    //% "Issuing authority"
    QT_TRID_NOOP("lc-euvrc-reg-issuing-auth");
    //% "Competent authority"
    QT_TRID_NOOP("lc-euvrc-reg-competent-auth");
    //% "Member state"
    QT_TRID_NOOP("lc-euvrc-reg-member-state");
    //% "Type approval number (K)"
    QT_TRID_NOOP("lc-euvrc-reg-type-approval");
    //% "Ownership status (C.4)"
    QT_TRID_NOOP("lc-euvrc-reg-ownership-status");
    //% "Previous document"
    QT_TRID_NOOP("lc-euvrc-reg-previous-document");

    // --- EU VRC vehicle field labels ---
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-veh-make");
    //% "Type (D.2)"
    QT_TRID_NOOP("lc-euvrc-veh-type");
    //% "Commercial description (D.3)"
    QT_TRID_NOOP("lc-euvrc-veh-commercial-desc");
    //% "VIN (E)"
    QT_TRID_NOOP("lc-euvrc-veh-vin");
    //% "Category (J)"
    QT_TRID_NOOP("lc-euvrc-veh-category");
    //% "Colour (R)"
    QT_TRID_NOOP("lc-euvrc-veh-colour");
    //% "Maximum speed (T)"
    QT_TRID_NOOP("lc-euvrc-veh-max-speed");

    // --- EU VRC engine & technical field labels ---
    //% "Engine capacity (P.1)"
    QT_TRID_NOOP("lc-euvrc-eng-capacity");
    //% "Maximum net power (P.2)"
    QT_TRID_NOOP("lc-euvrc-eng-max-power");
    //% "Type of fuel (P.3)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-type");
    //% "Engine ID number (P.5)"
    QT_TRID_NOOP("lc-euvrc-eng-id-number");
    //% "Vehicle mass (G)"
    QT_TRID_NOOP("lc-euvrc-eng-vehicle-mass");
    //% "Max permissible laden mass (F.1)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass");
    //% "Power/weight ratio (Q)"
    QT_TRID_NOOP("lc-euvrc-eng-power-weight");
    //% "Number of seats (S.1)"
    QT_TRID_NOOP("lc-euvrc-eng-seats");
    //% "Standing places (S.2)"
    QT_TRID_NOOP("lc-euvrc-eng-standing");
    //% "Number of axles (L)"
    QT_TRID_NOOP("lc-euvrc-eng-axles");
    //% "Wheelbase (M)"
    QT_TRID_NOOP("lc-euvrc-eng-wheelbase");
    //% "Max laden mass in service (F.2)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass-service");
    //% "Max laden mass whole vehicle (F.3)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass-whole");
    //% "Max braked trailer mass (O.1)"
    QT_TRID_NOOP("lc-euvrc-eng-braked-trailer");
    //% "Max unbraked trailer mass (O.2)"
    QT_TRID_NOOP("lc-euvrc-eng-unbraked-trailer");
    //% "Rated engine speed (P.4)"
    QT_TRID_NOOP("lc-euvrc-eng-rated-engine-speed");
    //% "Stationary sound level (U.1)"
    QT_TRID_NOOP("lc-euvrc-eng-stationary-sound");
    //% "Engine speed for sound ref (U.2)"
    QT_TRID_NOOP("lc-euvrc-eng-engine-speed-ref");
    //% "Drive-by sound level (U.3)"
    QT_TRID_NOOP("lc-euvrc-eng-drive-by-sound");
    //% "Fuel consumption (V.7)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-consumption");
    //% "CO2 emissions (V.7)"
    QT_TRID_NOOP("lc-euvrc-eng-co2");
    //% "Environmental category (V.9)"
    QT_TRID_NOOP("lc-euvrc-eng-env-category");
    //% "Fuel tank capacity (W)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-tank");

    // --- EU VRC holder field labels ---
    //% "Surname or business name (C.1.1)"
    QT_TRID_NOOP("lc-euvrc-holder-name");
    //% "Other names (C.1.2)"
    QT_TRID_NOOP("lc-euvrc-holder-other-names");
    //% "Address (C.1.3)"
    QT_TRID_NOOP("lc-euvrc-holder-address");

    // --- EU VRC user field labels ---
    //% "Surname or business name (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-name");
    //% "Other names (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-other-names");
    //% "Address (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-address");

    // --- EU VRC national extension labels (Serbian) ---
    //% "Owner Personal Number"
    QT_TRID_NOOP("lc-euvrc-nat-owners-personal-no");
    //% "User Personal Number"
    QT_TRID_NOOP("lc-euvrc-nat-users-personal-no");
    //% "Vehicle Load"
    QT_TRID_NOOP("lc-euvrc-nat-vehicle-load");
    //% "Year of Production"
    QT_TRID_NOOP("lc-euvrc-nat-year-of-production");
    //% "Serial Number"
    QT_TRID_NOOP("lc-euvrc-nat-serial-number");

    // --- EU VRC print document labels ---
    //% "EU Vehicle Registration Certificate"
    QT_TRID_NOOP("lc-euvrc-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-euvrc-doc-printing-date");
    //% "Registration number (A)"
    QT_TRID_NOOP("lc-euvrc-doc-reg-number");
    //% "Date of first registration (B)"
    QT_TRID_NOOP("lc-euvrc-doc-first-reg-date");
    //% "Registration date (I)"
    QT_TRID_NOOP("lc-euvrc-doc-reg-date");
    //% "Expiry date (H)"
    QT_TRID_NOOP("lc-euvrc-doc-expiry-date");
    //% "Member state"
    QT_TRID_NOOP("lc-euvrc-doc-member-state");
    //% "Document number"
    QT_TRID_NOOP("lc-euvrc-doc-document-number");
    //% "Competent authority"
    QT_TRID_NOOP("lc-euvrc-doc-competent-authority");
    //% "Issuing authority"
    QT_TRID_NOOP("lc-euvrc-doc-issuing-authority");
    //% "Type approval number (K)"
    QT_TRID_NOOP("lc-euvrc-doc-type-approval-no");
    //% "Vehicle Data"
    QT_TRID_NOOP("lc-euvrc-doc-vehicle-data");
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-doc-make");
    //% "Type (D.2)"
    QT_TRID_NOOP("lc-euvrc-doc-type");
    //% "Commercial description (D.3)"
    QT_TRID_NOOP("lc-euvrc-doc-commercial-desc");
    //% "VIN (E)"
    QT_TRID_NOOP("lc-euvrc-doc-vin");
    //% "Category (J)"
    QT_TRID_NOOP("lc-euvrc-doc-category");
    //% "Colour (R)"
    QT_TRID_NOOP("lc-euvrc-doc-colour");
    //% "Maximum speed (T)"
    QT_TRID_NOOP("lc-euvrc-doc-max-speed");
    //% "Engine & Technical"
    QT_TRID_NOOP("lc-euvrc-doc-engine-technical");
    //% "Engine capacity (P.1)"
    QT_TRID_NOOP("lc-euvrc-doc-capacity");
    //% "Maximum net power (P.2)"
    QT_TRID_NOOP("lc-euvrc-doc-power");
    //% "Type of fuel (P.3)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-type");
    //% "Engine ID number (P.5)"
    QT_TRID_NOOP("lc-euvrc-doc-engine-number");
    //% "Vehicle mass (G)"
    QT_TRID_NOOP("lc-euvrc-doc-mass");
    //% "Max permissible laden mass (F.1)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass");
    //% "Power/weight ratio (Q)"
    QT_TRID_NOOP("lc-euvrc-doc-power-weight");
    //% "Number of axles (L)"
    QT_TRID_NOOP("lc-euvrc-doc-axles");
    //% "Number of seats (S.1)"
    QT_TRID_NOOP("lc-euvrc-doc-seats");
    //% "Standing places (S.2)"
    QT_TRID_NOOP("lc-euvrc-doc-standing-places");
    //% "Wheelbase (M)"
    QT_TRID_NOOP("lc-euvrc-doc-wheelbase");
    //% "Holder Data"
    QT_TRID_NOOP("lc-euvrc-doc-holder-data");
    //% "Surname or business name (C.1.1)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-name");
    //% "Other names (C.1.2)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-other-names");
    //% "Address (C.1.3)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-address");
    //% "User Data"
    QT_TRID_NOOP("lc-euvrc-doc-user-data");
    //% "Surname or business name (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-name");
    //% "Other names (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-other-names");
    //% "Address (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-address");
    //% "Ownership status (C.4)"
    QT_TRID_NOOP("lc-euvrc-doc-ownership-status");
    //% "Previous document"
    QT_TRID_NOOP("lc-euvrc-doc-previous-document");
    //% "Owner Data"
    QT_TRID_NOOP("lc-euvrc-doc-owner-data");
    //% "Surname or business name (C.2)"
    QT_TRID_NOOP("lc-euvrc-doc-owner-name");
    //% "National Extensions"
    QT_TRID_NOOP("lc-euvrc-doc-national-data");
    //% "Max laden mass in service (F.2)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass-service");
    //% "Max laden mass whole vehicle (F.3)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass-whole");
    //% "Max braked trailer mass (O.1)"
    QT_TRID_NOOP("lc-euvrc-doc-braked-trailer");
    //% "Max unbraked trailer mass (O.2)"
    QT_TRID_NOOP("lc-euvrc-doc-unbraked-trailer");
    //% "Rated engine speed (P.4)"
    QT_TRID_NOOP("lc-euvrc-doc-rated-engine-speed");
    //% "Stationary sound level (U.1)"
    QT_TRID_NOOP("lc-euvrc-doc-stationary-sound");
    //% "Engine speed for sound ref (U.2)"
    QT_TRID_NOOP("lc-euvrc-doc-engine-speed-ref");
    //% "Drive-by sound level (U.3)"
    QT_TRID_NOOP("lc-euvrc-doc-drive-by-sound");
    //% "Fuel consumption (V.7)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-consumption");
    //% "CO2 emissions (V.7)"
    QT_TRID_NOOP("lc-euvrc-doc-co2");
    //% "Environmental category (V.9)"
    QT_TRID_NOOP("lc-euvrc-doc-env-category");
    //% "Fuel tank capacity (W)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-tank");

    //% "Vehicle registration card reader"
    // eMRTD (Electronic Passport)
    //% "Authentication Required"
    QT_TRID_NOOP("lc-emrtd-auth-required");
    //% "Authentication Failed"
    QT_TRID_NOOP("lc-emrtd-auth-failed");
    // clang-format off
    //% "Authentication failed — travel document data could not be read. Check CAN or MRZ and reinsert the card to try again."
    // clang-format on
    QT_TRID_NOOP("lc-emrtd-no-data-message");
    //% "Personal Data"
    QT_TRID_NOOP("lc-personal-data-title");
    //% "Surname"
    QT_TRID_NOOP("lc-emrtd-surname");
    //% "Given Names"
    QT_TRID_NOOP("lc-emrtd-given-names");
    //% "Nationality"
    QT_TRID_NOOP("lc-emrtd-nationality");
    //% "Date of Birth"
    QT_TRID_NOOP("lc-emrtd-date-of-birth");
    //% "Sex"
    QT_TRID_NOOP("lc-emrtd-document-data");
    //% "Document Number"
    QT_TRID_NOOP("lc-emrtd-doc-number");
    //% "Document Code"
    QT_TRID_NOOP("lc-emrtd-doc-code");
    //% "Issuing State"
    QT_TRID_NOOP("lc-emrtd-issuing-state");
    //% "Date of Expiry"
    QT_TRID_NOOP("lc-emrtd-date-of-expiry");
    //% "Personal Number"
    QT_TRID_NOOP("lc-emrtd-personal-number");
    //% "Additional Data"
    QT_TRID_NOOP("lc-emrtd-additional");
    //% "Issuing Information"
    QT_TRID_NOOP("lc-emrtd-issuing-info");
    //% "Electronic Passport Authentication"
    QT_TRID_NOOP("lc-emrtd-auth-can-title");
    //% "6-digit number on card front"
    QT_TRID_NOOP("lc-emrtd-auth-can-desc");
    //% "MRZ"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-title");
    //% "Data from document"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-desc");
    //% "Document number"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-docnum");
    //% "Date of birth"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-dob");
    //% "Date of expiry"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-expiry");
    //% "Authenticate"
    QT_TRID_NOOP("lc-emrtd-authenticate");
    //% "Authenticating..."
    QT_TRID_NOOP("lc-emrtd-authenticating");
    //% "CAN"
    QT_TRID_NOOP("lc-emrtd-auth-can-tab");
    //% "MRZ"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-tab");

    //% "Travel Document"
    QT_TRID_NOOP("lc-emrtd-travel-document");
    //% "Signature / Mark"
    QT_TRID_NOOP("lc-emrtd-signature");
    //% "Issuing Authority"
    QT_TRID_NOOP("lc-emrtd-issuing-authority");
    //% "Date of Issue"
    QT_TRID_NOOP("lc-emrtd-date-of-issue");
    //% "Endorsements"
    QT_TRID_NOOP("lc-emrtd-endorsements");
    //% "Tax/Exit Requirements"
    QT_TRID_NOOP("lc-emrtd-tax-exit");
    //% "Full Name"
    QT_TRID_NOOP("lc-emrtd-full-name");
    //% "Other Names"
    QT_TRID_NOOP("lc-emrtd-other-names");
    //% "Place of Birth"
    QT_TRID_NOOP("lc-emrtd-place-of-birth");
    //% "Address"
    QT_TRID_NOOP("lc-emrtd-address");
    //% "Telephone"
    QT_TRID_NOOP("lc-emrtd-telephone");
    //% "Profession"
    QT_TRID_NOOP("lc-emrtd-profession");
    //% "Title"
    QT_TRID_NOOP("lc-emrtd-title");
    //% "Custody Information"
    QT_TRID_NOOP("lc-emrtd-custody-info");

    // eMRTD print-only translations
    //% "TRAVEL DOCUMENT: DATA PRINTING"
    QT_TRID_NOOP("lc-emrtd-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-emrtd-doc-printing-date");
    //% "Sex"
    QT_TRID_NOOP("lc-emrtd-doc-sex");

    // --- eMRTD security status ---
    //% "Security Status"
    QT_TRID_NOOP("lc-emrtd-security-status");
    //% "Data Integrity"
    QT_TRID_NOOP("lc-emrtd-security-integrity");
    //% "Data Authenticity"
    QT_TRID_NOOP("lc-emrtd-security-authenticity");
    //% "Chip Genuineness"
    QT_TRID_NOOP("lc-emrtd-security-genuineness");
    //% "Passed"
    QT_TRID_NOOP("lc-emrtd-security-passed");
    //% "Failed"
    QT_TRID_NOOP("lc-emrtd-security-failed");
    //% "Not Supported"
    QT_TRID_NOOP("lc-emrtd-security-not-supported");
    //% "Skipped"
    QT_TRID_NOOP("lc-emrtd-security-skipped");
    //% "Not Performed"
    QT_TRID_NOOP("lc-emrtd-security-not-performed");
    //% "Detailed Checks"
    QT_TRID_NOOP("lc-emrtd-security-details");

    // --- eMRTD new DG sections ---
    //% "Portrait"
    QT_TRID_NOOP("lc-emrtd-portrait");
    //% "Contacts"
    QT_TRID_NOOP("lc-emrtd-contacts");
    //% "Contact Name"
    QT_TRID_NOOP("lc-emrtd-contact-name");
    //% "Fingerprint Biometrics"
    QT_TRID_NOOP("lc-emrtd-biometric-fingerprint");
    //% "Iris Biometrics"
    QT_TRID_NOOP("lc-emrtd-biometric-iris");
    //% "Access restricted (EAC required)"
    QT_TRID_NOOP("lc-emrtd-biometric-eac-required");
    //% "National Data"
    QT_TRID_NOOP("lc-emrtd-national-data");
    //% "Tag"
    QT_TRID_NOOP("lc-emrtd-national-tag");
    //% "Value"
    QT_TRID_NOOP("lc-emrtd-national-value");

    // --- PIV ---
    //% "PIV (NIST SP 800-73)"
    QT_TRID_NOOP("lc-piv-widget-title");
    //% "Print PIV Card"
    QT_TRID_NOOP("lc-piv-print-title");
    //% "PIV Card"
    QT_TRID_NOOP("lc-piv-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-piv-doc-printing-date");
    //% "CHUID"
    QT_TRID_NOOP("lc-piv-section-chuid");
    //% "CCC"
    QT_TRID_NOOP("lc-piv-section-ccc");
    //% "Printed Information"
    QT_TRID_NOOP("lc-piv-section-printed");
    //% "Discovery"
    QT_TRID_NOOP("lc-piv-section-discovery");
    //% "Key History"
    QT_TRID_NOOP("lc-piv-section-keyhistory");
    //% "GUID"
    QT_TRID_NOOP("lc-piv-field-guid");
    //% "FASC-N"
    QT_TRID_NOOP("lc-piv-field-fascn");
    //% "Expiration Date"
    QT_TRID_NOOP("lc-piv-field-expiration");
    //% "Card Identifier"
    QT_TRID_NOOP("lc-piv-field-cardid");
    //% "Name"
    QT_TRID_NOOP("lc-piv-field-name");
    //% "Employee Affiliation"
    QT_TRID_NOOP("lc-piv-field-affiliation");
    //% "Organization (Line 1)"
    QT_TRID_NOOP("lc-piv-field-org1");
    //% "Organization (Line 2)"
    QT_TRID_NOOP("lc-piv-field-org2");
    //% "Agency Serial Number"
    QT_TRID_NOOP("lc-piv-field-serial");
    //% "Issuer Identification"
    QT_TRID_NOOP("lc-piv-field-issuer");
    //% "PIN Policy"
    QT_TRID_NOOP("lc-piv-field-pinpolicy");
    //% "On-Card Certificates"
    QT_TRID_NOOP("lc-piv-field-oncardcerts");
    //% "Off-Card Certificates"
    QT_TRID_NOOP("lc-piv-field-offcardcerts");
    //% "Off-Card URL"
    QT_TRID_NOOP("lc-piv-field-offcardurl");

    // --- Signing UI ---
    //% "Sign"
    QT_TRID_NOOP("lc-sign-button");
    //% "Sign with this certificate"
    QT_TRID_NOOP("lc-sign-with-cert");
    //% "Sign Documents — %1"
    QT_TRID_NOOP("lc-sign-wizard-title");
    //% "Select files"
    QT_TRID_NOOP("lc-sign-select-files");
    //% "Formats: %1"
    QT_TRID_NOOP("lc-sign-formats-desc");
    //% "B-B (Basic)"
    QT_TRID_NOOP("lc-sign-level-bb");
    //% "B-T (with Timestamp)"
    QT_TRID_NOOP("lc-sign-level-bt");
    //% "B-LT (Long Term)"
    QT_TRID_NOOP("lc-sign-level-blt");
    //% "B-LTA (Long Term with Archive)"
    QT_TRID_NOOP("lc-sign-level-blta");
    //% "Output folder:"
    QT_TRID_NOOP("lc-sign-output-folder");
    //% "Change..."
    QT_TRID_NOOP("lc-sign-change-folder");
    //% "of %1"
    QT_TRID_NOOP("lc-sign-page-of-total");
    //% "Add visual signature"
    QT_TRID_NOOP("lc-sign-visual-sig");
    //% "PIN:"
    QT_TRID_NOOP("lc-sign-pin-label");
    //% "CAN:"
    QT_TRID_NOOP("lc-sign-can-label");
    //% "Sign"
    QT_TRID_NOOP("lc-sign-btn-sign");
    //% "Back"
    QT_TRID_NOOP("lc-sign-btn-back");
    //% "Next"
    QT_TRID_NOOP("lc-sign-btn-next");
    //% "Preparing to sign..."
    QT_TRID_NOOP("lc-sign-preparing");
    //% "Signing file %1 of %2: %3"
    QT_TRID_NOOP("lc-sign-progress");
    //% "Certificate"
    QT_TRID_NOOP("lc-sign-summary-cert");
    //% "Files"
    QT_TRID_NOOP("lc-sign-summary-files");
    //% "Signing complete: %1 succeeded, %2 failed"
    QT_TRID_NOOP("lc-sign-complete");
    //% "FAILED: %1 — %2"
    QT_TRID_NOOP("lc-sign-fail-sign");
    //% "FAILED: %1 — file exceeds 256 MB size limit"
    QT_TRID_NOOP("lc-sign-fail-too-large");
    //% "FAILED: signing was cancelled"
    QT_TRID_NOOP("lc-sign-fail-cancelled");
    //% "FAILED: incorrect PIN"
    QT_TRID_NOOP("lc-sign-fail-pin");
    //% "FAILED: card is blocked"
    QT_TRID_NOOP("lc-sign-fail-blocked");
    //% "FAILED: timestamp authority unreachable"
    QT_TRID_NOOP("lc-sign-fail-tsa");
    //% "FAILED: trust store unavailable"
    QT_TRID_NOOP("lc-sign-fail-trust");
    //% "FAILED: invalid signing request"
    QT_TRID_NOOP("lc-sign-fail-invalid");
    //% "FAILED: signing failed — see log for details"
    QT_TRID_NOOP("lc-sign-fail-generic");
    //% "OK: %1 → %2"
    QT_TRID_NOOP("lc-sign-ok");
    //% "Previous"
    QT_TRID_NOOP("lc-sign-page-prev");
    //% "Next"
    QT_TRID_NOOP("lc-sign-page-next");
    //% "Reason:"
    QT_TRID_NOOP("lc-sign-visual-reason");
    //% "e.g. Contract approval"
    QT_TRID_NOOP("lc-sign-visual-reason-placeholder");
    //% "Location:"
    QT_TRID_NOOP("lc-sign-visual-location");
    //% "e.g. Belgrade, Serbia"
    QT_TRID_NOOP("lc-sign-visual-location-placeholder");
    //% "Select output folder"
    QT_TRID_NOOP("lc-sign-select-output-folder");
    //% "Signature level:"
    QT_TRID_NOOP("lc-sign-level-label");
    //% "TSA server:"
    QT_TRID_NOOP("lc-sign-tsa-label");
    //% "For a qualified signature, use a qualified TSA from your certificate authority"
    QT_TRID_NOOP("lc-sign-tsa-info");
    //% "Add TSA server"
    QT_TRID_NOOP("lc-sign-tsa-add-title");
    //% "Enter TSA server URL:"
    QT_TRID_NOOP("lc-sign-tsa-add-prompt");
    //% "+ Add TSA server..."
    QT_TRID_NOOP("lc-sign-tsa-add-item");
    //% "Cancel"
    QT_TRID_NOOP("lc-sign-btn-cancel");
    //% "Done"
    QT_TRID_NOOP("lc-sign-btn-done");
    //% "Copy"
    QT_TRID_NOOP("lc-sign-copy-error");
    //% "Max file size: 256 MB. ZIP archives for ASiC-E: up to 100,000 files, 256 MB decompressed."
    QT_TRID_NOOP("lc-sign-limits-info");
    //% "Select a file and press Delete to remove it"
    QT_TRID_NOOP("lc-sign-filelist-tooltip");

    // Visual signature text labels (embedded in signed PDF)
    //% "Digitally signed by:"
    QT_TRID_NOOP("lc-sign-visual-text-signed-by");
    //% "Issued by:"
    QT_TRID_NOOP("lc-sign-visual-text-issued-by");
    //% "Date:"
    QT_TRID_NOOP("lc-sign-visual-text-date");
    //% "Reason:"
    QT_TRID_NOOP("lc-sign-visual-text-reason");
    //% "Location:"
    QT_TRID_NOOP("lc-sign-visual-text-location");

    // Wizard header
    //% "Sign Documents"
    QT_TRID_NOOP("lc-sign-wizard-header-title");
    //% "Files"
    QT_TRID_NOOP("lc-sign-step-files");
    //% "Place"
    QT_TRID_NOOP("lc-sign-step-place");
    //% "Sign"
    QT_TRID_NOOP("lc-sign-step-sign");

    // Drop zone
    //% "Drop PDF or other files here"
    QT_TRID_NOOP("lc-sign-drop-primary");
    //% "or"
    QT_TRID_NOOP("lc-sign-drop-or");
    //% "browse files"
    QT_TRID_NOOP("lc-sign-drop-browse");

    // Sign page summary
    //% "Level"
    QT_TRID_NOOP("lc-sign-summary-level");

    // Expired certificate warning
    //% "Expired Certificate"
    QT_TRID_NOOP("lc-sign-expired-cert-title");
    //% "The selected certificate has expired. At B-B level, the signature will not include revocation "
    //% "data. Continue anyway?"
    QT_TRID_NOOP("lc-sign-expired-cert-message");
    //% "Output Folder Error"
    QT_TRID_NOOP("lc-sign-output-folder-error-title");
    //% "Could not create the output folder."
    QT_TRID_NOOP("lc-sign-output-folder-error-message");
    //% "Invalid TSA URL"
    QT_TRID_NOOP("lc-sign-tsa-invalid-title");
    //% "The configured Timestamp Authority URL is not a valid https:// endpoint."
    QT_TRID_NOOP("lc-sign-tsa-invalid-message");
    //% "Unknown signing error"
    QT_TRID_NOOP("lc-sign-unknown-error");

    // --- menu bar ---
    //% "Edit"
    QT_TRID_NOOP("lc-menu-edit");
    //% "Settings..."
    QT_TRID_NOOP("lc-menu-settings");
    //% "Help"
    QT_TRID_NOOP("lc-menu-help");
    //% "About LibreCelik"
    QT_TRID_NOOP("lc-menu-about");
    //% "About Qt"
    QT_TRID_NOOP("lc-menu-about-qt");
    //% "Services"
    QT_TRID_NOOP("lc-menu-services");
    //% "Hide LibreCelik"
    QT_TRID_NOOP("lc-menu-hide");
    //% "Hide Others"
    QT_TRID_NOOP("lc-menu-hide-others");
    //% "Show All"
    QT_TRID_NOOP("lc-menu-show-all");
    //% "Quit LibreCelik"
    QT_TRID_NOOP("lc-menu-quit");
    //% "About LibreCelik"
    QT_TRID_NOOP("lc-about-title");

    // --- settings dialog ---
    //% "Settings"
    QT_TRID_NOOP("lc-settings-title");
    //% "General"
    QT_TRID_NOOP("lc-settings-tab-general");
    //% "Signing"
    QT_TRID_NOOP("lc-settings-tab-signing");
    //% "Trust"
    QT_TRID_NOOP("lc-settings-tab-trust");
    //% "Language:"
    QT_TRID_NOOP("lc-settings-language");
    //% "Default signature level:"
    QT_TRID_NOOP("lc-settings-default-level");
    //% "Default output folder:"
    QT_TRID_NOOP("lc-settings-default-output");
    //% "Same as input file"
    QT_TRID_NOOP("lc-settings-output-placeholder");
    //% "TSA servers:"
    QT_TRID_NOOP("lc-settings-tsa-servers");
    //% "Trusted Lists:"
    QT_TRID_NOOP("lc-settings-tl-servers");
    //% "+ Add Trusted List..."
    QT_TRID_NOOP("lc-settings-tl-add-item");
    //% "Add Trusted List"
    QT_TRID_NOOP("lc-settings-tl-add-title");
    //% "Type:"
    QT_TRID_NOOP("lc-settings-tl-type");
    //% "Loading:"
    QT_TRID_NOOP("lc-settings-tl-loading");
    //% "Cache directory:"
    QT_TRID_NOOP("lc-settings-cache-dir");
    //% "Invalid URL"
    QT_TRID_NOOP("lc-settings-invalid-url-title");
    //% "Please enter a valid URL."
    QT_TRID_NOOP("lc-settings-invalid-url-msg");

    // Standard QDialogButtonBox button labels — registered to override KDE
    // Plasma's kwidgetsaddons6_qt.qm translator that auto-loads on the Qt
    // platform integration and translates these to Cyrillic Serbian
    // regardless of the app's selected locale. Applied via
    // librecelik::ButtonBox::retranslateUi (src/utils/buttonbox.h) for
    // explicit QDialogButtonBox uses, and librecelik::dialogs::*
    // (src/utils/dialogs.h) for QMessageBox / QInputDialog convenience
    // calls.
    //% "OK"
    QT_TRID_NOOP("lc-btn-ok");
    //% "Cancel"
    QT_TRID_NOOP("lc-btn-cancel");
    //% "Close"
    QT_TRID_NOOP("lc-btn-close");
    //% "Apply"
    QT_TRID_NOOP("lc-btn-apply");
    //% "Save"
    QT_TRID_NOOP("lc-btn-save");
    //% "Reset"
    QT_TRID_NOOP("lc-btn-reset");
    //% "Restore Defaults"
    QT_TRID_NOOP("lc-btn-restore-defaults");
    //% "Yes"
    QT_TRID_NOOP("lc-btn-yes");
    //% "No"
    QT_TRID_NOOP("lc-btn-no");
    //% "Help"
    QT_TRID_NOOP("lc-btn-help");
    //% "Abort"
    QT_TRID_NOOP("lc-btn-abort");
    //% "Retry"
    QT_TRID_NOOP("lc-btn-retry");
    //% "Ignore"
    QT_TRID_NOOP("lc-btn-ignore");
    //% "Discard"
    QT_TRID_NOOP("lc-btn-discard");

    // --- signing wizard PKCS#11 slot labels (multi-PIN cards) ---
    // Used by librecelik::signing::formatSlotLabel to render one
    // dropdown entry per PKCS#11 slot for cards exposing multiple
    // PINs (e.g. Serbian GEO eID Auth + Signing (QSCD) slots).
    //% "Authentication"
    QT_TRID_NOOP("lc-pin-label-auth");
    //% "Signing (QSCD)"
    QT_TRID_NOOP("lc-pin-label-qscd");
    //% "Signing"
    QT_TRID_NOOP("lc-pin-label-sign");
}

QT_WARNING_POP
