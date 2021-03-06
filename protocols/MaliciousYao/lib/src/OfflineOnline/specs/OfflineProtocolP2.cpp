#include "../../../include/OfflineOnline/specs/OfflineProtocolP2.hpp"

OfflineProtocolP2::OfflineProtocolP2(const shared_ptr<ExecutionParameters> & mainExecution, const shared_ptr<ExecutionParameters> & crExecution, 
	const shared_ptr<CommunicationConfig> & communication, const shared_ptr<OTBatchReceiver> & maliciousOtReceiver, bool writeToFile)
{
	this->mainExecution = mainExecution;
	this->crExecution = crExecution;
	this->channel = communication->getCommParty();
	this->maliciousOtReceiver = maliciousOtReceiver;
	this->writeToFile = writeToFile;
}

void OfflineProtocolP2::run()
{
	//LogTimer timer("Offline protocol P2");
	int crInputSizeY = CryptoPrimitives::getAES()->getBlockSize() * 8;
	//timer.reset("selecting and sending probe resistant matrices");
	// Selecting E and sending it to P1.
	mainMatrix = selectAndSendProbeResistantMatrix(mainExecution);
	// Selecting E' and sending it to P1 (derive the length of the new input from the MES key size - that is the size of proofOfCheating).
	crMatrix = selectAndSendProbeResistantMatrix(crInputSizeY, crExecution->getStatisticalParameter());
	//timer.stop();

	//timer.reset("runCutAndChooseProtocol(AES)");
	//Create the main bundleBuilder from the main circuit.
	//Use the first circuit only because there is no use of thread in this party and therefore, only one circuit is needed.
	auto mainBundleBuilder = make_shared<BundleBuilder>(mainExecution->getCircuit(0), mainMatrix);

	//Run Cut and Choose protocol on the main circuit.
	mainBuckets = runCutAndChooseProtocol(mainExecution, mainBundleBuilder, ((writeToFile == false) ? "" : "main"));
	//timer.stop();

	//timer.reset("runCutAndChooseProtocol(CR)");
	//Create the cheating recovery bundleBuilder from the main circuit.
	//Use the first circuit only because there is no use of thread in this party and therefore, only one circuit is needed.
	auto tempKey = CryptoPrimitives::getAES()->generateKey(KEY_SIZE);
	auto crBundleBuilder = make_shared<CheatingRecoveryBundleBuilder>(crExecution->getCircuit(0), crMatrix,	tempKey);

	//Run Cut and Choose protocol on the cheating recovery circuit.
	crBuckets = runCutAndChooseProtocol(crExecution, crBundleBuilder, ((writeToFile == false) ? "" : "cr"), crInputSizeY);
	//timer.stop();

	//timer.reset("runObliviousTransferOnP2Keys(AES)");
	//Run OT on p2 keys of the main circuit.
	runObliviousTransferOnP2Keys(mainExecution, mainMatrix, mainBuckets);
	//timer.stop();
	//timer.reset("runObliviousTransferOnP2Keys(CR)");
	//Run OT on p2 keys of the cheating recovery circuit.
	runObliviousTransferOnP2Keys(crExecution, crMatrix, crBuckets);
	//timer.stop();
}

vector<int> OfflineProtocolP2::getSecretSharingLabels(int crInputSizeY)
{
	vector<int> labels(crInputSizeY);
	iota(labels.begin(), labels.end(), 1);
	return labels;
}

shared_ptr<KProbeResistantMatrix> OfflineProtocolP2::selectAndSendProbeResistantMatrix(shared_ptr<ExecutionParameters> execution)
{
	auto inputLabelsP2Size = execution->getCircuit(0)->getNumberOfInputs(2);

	return selectAndSendProbeResistantMatrix(inputLabelsP2Size, execution->getStatisticalParameter());
}

shared_ptr<KProbeResistantMatrix> OfflineProtocolP2::selectAndSendProbeResistantMatrix(int n, int s)
{
	auto matrix = make_shared<KProbeResistantMatrix>(n, s);
	sendSerialize(matrix, channel[0].get());
	return matrix;
}

shared_ptr<BucketLimitedBundleList> OfflineProtocolP2::runCutAndChooseProtocol(shared_ptr<ExecutionParameters> execution, 
	shared_ptr<BundleBuilder> bundleBuilder, string garbledTablesFilePrefix, int inputLabelsY2Size)
{
	//Create the cut and choose verifier.
	CutAndChooseVerifier verifier = CutAndChooseVerifier(execution, channel, bundleBuilder, garbledTablesFilePrefix, inputLabelsY2Size);
	//Run the cut and choose protocol.
	verifier.run();
	//Return the buckets that were generated in the cut and choose protocol.
	return verifier.getBuckets();
}

void OfflineProtocolP2::runObliviousTransferOnP2Keys(shared_ptr<ExecutionParameters> execution, shared_ptr<KProbeResistantMatrix> matrix, shared_ptr<BucketLimitedBundleList> buckets)
{
	//Create and run malicious OT routine.
	OfflineOtReceiverRoutine otReceiver(execution, maliciousOtReceiver, matrix, buckets);
	otReceiver.run();
}
