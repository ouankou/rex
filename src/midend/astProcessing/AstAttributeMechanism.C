#include "AstAttributeMechanism.h"

#include "sage3basic.h"

#include "roseInternal.h"

#include <cstddef>
#include <memory>
#include <sstream>

using namespace Rose;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      AstAttributeMechanism
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string attributeFullName(const std::string &name,
                                     AstAttribute *value) {
  ROSE_ASSERT(value != nullptr);
  return "attribute " + name + "(" + value->toString() + ") of " +
         value->attribute_class_name() + " type";
}

static void deleteAttributeValue(const std::string &name, AstAttribute *value) {
  if (value != NULL) {
    switch (value->getOwnershipPolicy()) {
    case AstAttribute::CONTAINER_OWNERSHIP:
      delete value;
      break;
    case AstAttribute::NO_OWNERSHIP:
      MLOG_WARN_CXX("midend:astProcessing")
          << attributeFullName(name, value) << " is leaked\n";
      break;
    case AstAttribute::CUSTOM_OWNERSHIP:
      break;
    case AstAttribute::UNKNOWN_OWNERSHIP:
      MLOG_WARN_CXX("midend:astProcessing")
          << attributeFullName(name, value)
          << " has unknown ownership and may leak\n";
      break;
    }
  }
}

AstAttributeMechanism &
AstAttributeMechanism::operator=(const AstAttributeMechanism &other) {
  assignFrom(other);
  return *this;
}

AstAttributeMechanism::~AstAttributeMechanism() { clearAttributes(); }

void AstAttributeMechanism::clearAttributes() {
  for (const auto &entry : attributes_) {
    deleteAttributeValue(entry.first, entry.second);
  }
  attributes_.clear();
}

bool AstAttributeMechanism::exists(const std::string &name) const {
  return attributes_.find(name) != attributes_.end();
}

void AstAttributeMechanism::set(const std::string &name,
                                AstAttribute *newValue) {
  auto it = attributes_.find(name);
  if (it != attributes_.end()) {
    AstAttribute *oldValue = it->second;
    ROSE_ASSERT(oldValue != newValue);
    deleteAttributeValue(name, oldValue);
    if (newValue != NULL) {
      it->second = newValue;
    } else
      attributes_.erase(it);
  } else if (newValue != NULL) {
    attributes_.insert(std::make_pair(name, newValue));
  }
}

bool AstAttributeMechanism::add(const std::string &name, AstAttribute *value) {
  auto it = attributes_.find(name);
  if (it != attributes_.end()) {
    ROSE_ASSERT(it->second != value);
    deleteAttributeValue(name, value);
    return false;
  } else if (value != NULL) {
    attributes_.insert(std::make_pair(name, value));
    return true;
  }
  return false;
}

bool AstAttributeMechanism::replace(const std::string &name,
                                    AstAttribute *value) {
  auto it = attributes_.find(name);
  if (it != attributes_.end()) {
    AstAttribute *oldValue = it->second;
    ROSE_ASSERT(oldValue != value);
    deleteAttributeValue(name, oldValue);
    if (value != NULL)
      it->second = value;
    else
      attributes_.erase(it);
    return true;
  }
  deleteAttributeValue(name, value);
  return false;
}

AstAttribute *AstAttributeMechanism::operator[](const std::string &name) const {
  auto it = attributes_.find(name);
  if (it != attributes_.end())
    return it->second;
  return NULL;
}

void AstAttributeMechanism::remove(const std::string &name) {
  attributeMap_::iterator it = attributes_.find(name);
  if (it != attributes_.end()) {
    AstAttribute *oldValue = (*it).second;
    deleteAttributeValue(name, oldValue);
    attributes_.erase(it);
  }
}

AstAttributeMechanism::AttributeIdentifiers
AstAttributeMechanism::getAttributeIdentifiers() const {
  AttributeIdentifiers retval;
  for (const auto &entry : attributes_)
    retval.insert(entry.first);
  return retval;
}

size_t AstAttributeMechanism::size() const { return attributes_.size(); }

// Construction and assignment. Must be exception-safe.
void AstAttributeMechanism::assignFrom(const AstAttributeMechanism &other) {
  if (this == &other)
    return;
  AstAttributeMechanism tmp;
  for (auto it = other.attributes_.begin(); it != other.attributes_.end();
       ++it) {
    AstAttribute *attr = it->second;
    ASSERT_not_null(attr);

    // Keep a copied value owned until the temporary container accepts it.
    auto deletePending = [attr](AstAttribute *value) {
      if (value && value != attr &&
          value->getOwnershipPolicy() == AstAttribute::CONTAINER_OWNERSHIP)
        delete value;
    };
    std::unique_ptr<AstAttribute, decltype(deletePending)> pending(
        attr->copy(), deletePending);
    AstAttribute *copied = pending.get();

    switch (attr->getOwnershipPolicy()) {
    case AstAttribute::CONTAINER_OWNERSHIP:
      ASSERT_require2(
          copied != attr,
          "virtual copy function for \"%s\" did not copy attribute\n",
          attributeFullName(it->first, it->second).c_str());
      break;
    case AstAttribute::NO_OWNERSHIP:
    case AstAttribute::CUSTOM_OWNERSHIP:
      break;
    case AstAttribute::UNKNOWN_OWNERSHIP:
      // Similar to CONTAINER_OWNERSHIP but only warn, don't assert.
      if (copied == attr)
        MLOG_WARN_CXX("midend:astProcessing")
            << attributeFullName(it->first, it->second)
            << " virtual copy function did not copy the attribute (unknown "
               "ownership)\n";
      break;
    }

    if (copied) {
      tmp.set(it->first, copied);
      pending.release();
    }
  }
  std::swap(attributes_, tmp.attributes_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      AstAttribute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AstAttribute::OwnershipPolicy AstAttribute::getOwnershipPolicy() const {
  return UNKNOWN_OWNERSHIP;
}

std::string AstAttribute::toString() {
  return StringUtility::numberToString((void *)(this));
}

std::string AstAttribute::additionalNodeOptions() { return ""; }

std::vector<AstAttribute::AttributeEdgeInfo>
AstAttribute::additionalEdgeInfo() {
  return std::vector<AttributeEdgeInfo>();
}

std::vector<AstAttribute::AttributeNodeInfo>
AstAttribute::additionalNodeInfo() {
  return std::vector<AttributeNodeInfo>();
}

bool AstAttribute::commentOutNodeInGraph() { return false; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      MetricAttribute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AstAttribute *MetricAttribute::copy() const {
  return new MetricAttribute(*this);
}

std::string MetricAttribute::attribute_class_name() const {
  return "MetricAttribute";
}

MetricAttribute &MetricAttribute::operator+=(const MetricAttribute &other) {
  is_derived_ = true;
  value_ += other.value_;
  return *this;
}

MetricAttribute &MetricAttribute::operator-=(const MetricAttribute &other) {
  is_derived_ = true;
  value_ -= other.value_;
  return *this;
}

MetricAttribute &MetricAttribute::operator*=(const MetricAttribute &other) {
  is_derived_ = true;
  value_ *= other.value_;
  return *this;
}

MetricAttribute &MetricAttribute::operator/=(const MetricAttribute &other) {
  is_derived_ = true;
  value_ /= other.value_;
  return *this;
}

std::string MetricAttribute::toString() {
  std::ostringstream ostr;
  ostr << value_;
  std::string retval = ostr.str();

  if (is_derived_)
    retval += "**";
  return retval;
}
