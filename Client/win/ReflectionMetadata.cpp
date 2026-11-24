#include "ReflectionMetadata.h"

#include "v8xml/SerializerV2.h"
#include "v8xml/XmlSerializer.h"

#include <G3D/System.h>
#ifdef __ANDROID__
#include "AndroidAssets.h"
#endif
#include <filesystem>
#include <fstream>

namespace RBX::Reflection::Metadata {

char sReflection[] = "ReflectionMetadata";

char sClasses[] = "ReflectionMetadataClasses";

char sItem[] = "ReflectionMetadataItem";

char sClass[] = "ReflectionMetadataClass";

char sProperties[] = "ReflectionMetadataProperties";

char sFunctions[] = "ReflectionMetadataFunctions";

char sEvents[] = "ReflectionMetadataEvents";

char sMember[] = "ReflectionMetadataMember";

bool Reflection::askAddChild(const Instance* instance) const
{
	return dynamic_cast<const Classes*>(instance) != nullptr;
}

bool Classes::askAddChild(const Instance* instance) const
{
	return dynamic_cast<const Class*>(instance) != nullptr;
}

bool Class::askAddChild(const Instance* instance) const
{
	return dynamic_cast<const Members*>(instance) != nullptr;
}

bool Members::askAddChild(const Instance* instance) const
{
	return dynamic_cast<const Member*>(instance) != nullptr;
}

Class* Classes::findClass(const ClassDescriptor* descriptor, bool recurse)
{
	Class* result = static_cast<Class*>(findFirstChildByName(descriptor->name.toString()));
	if (result) {
		return result;
	}
	if (recurse && descriptor->getBase()) {
		return findClass(descriptor->getBase(), true);
	}
	return nullptr;
}

Class* Reflection::getClass(const ClassDescriptor* descriptor, bool recurse)
{
	if (m_classes) {
		return m_classes->findClass(descriptor, recurse);
	}
	return nullptr;
}

Member* Reflection::getProperty(const PropertyDescriptor* descriptor)
{
	Class* item = m_classes ? m_classes->findClass(&descriptor->owner, false) : nullptr;
	if (item) {
		Properties* members = item->findFirstChildOfType<Properties>();
		if (members) {
			return static_cast<Member*>(members->findFirstChildByName(descriptor->name.toString()));
		}
	}
	return nullptr;
}

Member* Reflection::getFunction(const FunctionDescriptor* descriptor)
{
	Class* item = m_classes ? m_classes->findClass(&descriptor->owner, false) : nullptr;
	if (item) {
		Functions* members = item->findFirstChildOfType<Functions>();
		if (members) {
			return static_cast<Member*>(members->findFirstChildByName(descriptor->name.toString()));
		}
	}
	return nullptr;
}

Member* Reflection::getEvent(const SignalDescriptor* descriptor)
{
	Class* item = m_classes ? m_classes->findClass(&descriptor->owner, false) : nullptr;
	if (item) {
		Events* members = item->findFirstChildOfType<Events>();
		if (members) {
			return static_cast<Member*>(members->findFirstChildByName(descriptor->name.toString()));
		}
	}
	return nullptr;
}

void registerClasses()
{
	Reflection::classDescriptor();
	Classes::classDescriptor();
	Functions::classDescriptor();
	Member::classDescriptor();
	Properties::classDescriptor();
	Events::classDescriptor();
}

Reflection::Reflection() : m_classes(nullptr)
{
}

void Classes::addHiddenClass(const ClassDescriptor& descriptor)
{
	Instance* existing = findFirstChildByName(descriptor.name.toString());
	if (existing) {
		static_cast<Item*>(existing)->browsable = false;
		return;
	}

	std::shared_ptr<Class> metadata = Creatable<Instance>::create<Class>();
	metadata->setName(descriptor.name.toString());
	metadata->browsable = false;
	metadata->setParent(this);
}

void Reflection::load(std::string fileName)
{
	if (fileName.size() == 0) {
		return;
	}

	std::ifstream stream(fileName.c_str(), std::ios_base::in | std::ios_base::binary);
	TextXmlParser parser(stream.rdbuf());
	std::unique_ptr<XmlElement> root(parser.parse());

	MergeBinder binder;
	readChildren(root.get(), binder);
	binder.resolveRefs();

	m_classes = findFirstChildOfType<Classes>();
	if (m_classes) {
		m_classes->addHiddenClass(ClassDescriptor::rootDescriptor());
		m_classes->addHiddenClass(Instance::classDescriptor());
		m_classes->addHiddenClass(Reflection::classDescriptor());
		m_classes->addHiddenClass(Classes::classDescriptor());
		m_classes->addHiddenClass(Item::classDescriptor());
		m_classes->addHiddenClass(Class::classDescriptor());
		m_classes->addHiddenClass(Member::classDescriptor());
		m_classes->addHiddenClass(Properties::classDescriptor());
		m_classes->addHiddenClass(Functions::classDescriptor());
		m_classes->addHiddenClass(Events::classDescriptor());
	}
}

std::shared_ptr<Reflection> Reflection::singleton()
{
	static std::shared_ptr<Reflection> reflection;
	if (!reflection) {
		reflection = Creatable<Instance>::create<Reflection>();

#ifdef __ANDROID__
		std::filesystem::path fileName = AndroidAssets::dataDirectory();
#else
		std::filesystem::path fileName = std::filesystem::path(G3D::System::currentProgramFilename()).parent_path();
#endif
		reflection->load((fileName / "ReflectionMetadata.xml").string().c_str());
	}
	return reflection;
}

RBX::Reflection::BoundProp<bool, 1> Item::prop_Browsable("Browsable", "Reflection", &Item::browsable);

RBX::Reflection::BoundProp<bool, 1> Item::prop_Deprecated("Deprecated", "Reflection", &Item::deprecated);

RBX::Reflection::BoundProp<bool, 1> Item::prop_Preliminary("Preliminary", "Reflection", &Item::preliminary);

RBX::Reflection::BoundProp<std::string, 1> Item::prop_summary("summary", "Reflection", &Item::summary);

RBX::Reflection::BoundProp<int, 1> Class::prop_ExplorerOrder("ExplorerOrder", "Reflection", &Class::explorerOrder);

RBX::Reflection::BoundProp<int, 1> Class::prop_ExplorerImageIndex(
	"ExplorerImageIndex", "Reflection", &Class::explorerImageIndex
);

} // namespace RBX::Reflection::Metadata

template class RBX::
	FactoryProduct<RBX::Reflection::Metadata::Reflection, RBX::Instance, RBX::Reflection::Metadata::sReflection>;
template class RBX::
	DescribedCreatable<RBX::Reflection::Metadata::Reflection, RBX::Instance, RBX::Reflection::Metadata::sReflection>;
template class RBX::
	FactoryProduct<RBX::Reflection::Metadata::Classes, RBX::Instance, RBX::Reflection::Metadata::sClasses>;
template class RBX::
	DescribedCreatable<RBX::Reflection::Metadata::Classes, RBX::Instance, RBX::Reflection::Metadata::sClasses>;
template class RBX::FactoryProduct<
	RBX::Reflection::Metadata::Class,
	RBX::Reflection::Metadata::Item,
	RBX::Reflection::Metadata::sClass>;
template class RBX::DescribedCreatable<
	RBX::Reflection::Metadata::Class,
	RBX::Reflection::Metadata::Item,
	RBX::Reflection::Metadata::sClass>;
template class RBX::FactoryProduct<
	RBX::Reflection::Metadata::Properties,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sProperties>;
template class RBX::DescribedCreatable<
	RBX::Reflection::Metadata::Properties,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sProperties>;
template class RBX::FactoryProduct<
	RBX::Reflection::Metadata::Functions,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sFunctions>;
template class RBX::DescribedCreatable<
	RBX::Reflection::Metadata::Functions,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sFunctions>;
template class RBX::FactoryProduct<
	RBX::Reflection::Metadata::Events,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sEvents>;
template class RBX::DescribedCreatable<
	RBX::Reflection::Metadata::Events,
	RBX::Reflection::Metadata::Members,
	RBX::Reflection::Metadata::sEvents>;
template class RBX::FactoryProduct<
	RBX::Reflection::Metadata::Member,
	RBX::Reflection::Metadata::Item,
	RBX::Reflection::Metadata::sMember>;
template class RBX::DescribedCreatable<
	RBX::Reflection::Metadata::Member,
	RBX::Reflection::Metadata::Item,
	RBX::Reflection::Metadata::sMember>;
